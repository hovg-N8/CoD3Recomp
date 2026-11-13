using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Media;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Cod3Launcher {
 public sealed class LauncherPreferences { public string gameFolder = ""; public bool ambientAudio = true; public string soundtrack; public bool rotateArtwork = true; }
 public sealed class LauncherForm : Form {
  const string Service = "https://cod3-updates.hovgtv.workers.dev";
  readonly Label status = new Label(); readonly Label versions = new Label(); readonly Label folder = new Label();
  readonly TextBox notes = new TextBox(); readonly ProgressBar progress = new ProgressBar();
  readonly Button campaign = new Button(); readonly Button multiplayer = new Button(); readonly Button repair = new Button(); readonly Button browse = new Button();
  readonly ComboBox soundtrack = new ComboBox();
  readonly CheckBox rotateArtwork = new CheckBox();
  readonly Button checkUpdates = new Button();
  readonly Button patchLog = new Button();
  readonly Label sceneName = new Label();
  readonly Timer artworkTimer = new Timer { Interval = 12000 };
  readonly string[] sceneResources = { "Battlefield", "SaintLo", "Falaise", "Chambois", "Ridge", "Railway", "Farm", "Quarry", "VillageBarrage", "TyphoonPass" };
  readonly string[] sceneTitles = { "Orchard convoy at night", "Saint-Lô in the rain", "Falaise armored column", "Chambois crossing", "Polish armor at dawn", "Railway yard at dawn", "Normandy farm at dusk", "Quarry field post", "Village under fire", "Typhoons over Normandy" };
  int sceneIndex;
  Release publishedRelease;
  Image battlefield;Image jarcLogo;Image treyarchLogo;Stream ambienceStream;SoundPlayer sound;AmbienceController ambience;
  LauncherPreferences preferences=new LauncherPreferences();bool restoringPreferences;Process launchedGame;
  readonly bool preview; string root = ""; bool busy; bool launchBlocked=true;
  readonly string preferenceFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"HOVGtv","CoD3Launcher","settings.json");
  public LauncherForm(bool previewMode) {
   preview=previewMode; Text="Call of Duty 3 | Launcher V3"; Name="CoD3Launcher";
   ClientSize=new Size(1120,740); MinimumSize=new Size(1136,779); StartPosition=FormStartPosition.CenterScreen;
   DoubleBuffered=true;
   using(var resource=typeof(LauncherForm).Assembly.GetManifestResourceStream("Cod3Launcher.Battlefield"))if(resource!=null)using(var source=Image.FromStream(resource))battlefield=new Bitmap(source);
   jarcLogo=LoadArtwork("Cod3Launcher.JarcLogo");treyarchLogo=LoadArtwork("Cod3Launcher.TreyarchLogo");
   ambience = new AmbienceController(PlaySoundtrack, StopSoundtrack);
   BackColor=Color.FromArgb(19,22,21); ForeColor=Color.FromArgb(232,235,225); Font=new Font("Segoe UI",10); AutoScaleMode=AutoScaleMode.Dpi;
   var title=new Label {Text="CALL OF DUTY 3",Font=new Font("Segoe UI",28,FontStyle.Bold),AutoSize=true,Location=new Point(26,28),BackColor=Color.Transparent,ForeColor=Color.FromArgb(242,234,210)};
   var subtitle=new Label {Text="PC PORT  /  COMMUNITY LAUNCHER",Font=new Font("Segoe UI",10,FontStyle.Bold),ForeColor=Color.FromArgb(181,192,135),AutoSize=true,Location=new Point(34,83)};
   Controls.Add(title); Controls.Add(subtitle);
   versions.SetBounds(34,126,340,46); versions.Text="Installed build: -\r\nPublished build: -"; versions.BackColor=Color.Transparent; Controls.Add(versions);
   folder.SetBounds(34,177,336,42); folder.AutoEllipsis=true; folder.ForeColor=Color.Silver; folder.BackColor=Color.Transparent; folder.Text="Choose your existing COD3 PC port folder."; Controls.Add(folder);
   SetupButton(browse,"Choose folder",new Rectangle(34,213,155,36),Color.FromArgb(51,58,51));
   SetupButton(checkUpdates, "Check for Updates", new Rectangle(34, 394, 164, 40), Color.FromArgb(75, 77, 53));
   SetupButton(patchLog, "Patch Log", new Rectangle(206, 394, 164, 40), Color.FromArgb(75, 77, 53));
   soundtrack.Name = "Soundtrack";
   soundtrack.AccessibleName = "Launcher soundtrack";
   soundtrack.DropDownStyle = ComboBoxStyle.DropDownList;
   soundtrack.Items.AddRange(new object[] { "Frontline Mix", "Battle Ambience", "Sound Off" });
   soundtrack.SetBounds(34, 495, 177, 30);
   soundtrack.SelectedIndex = 2;
   Controls.Add(soundtrack);
   rotateArtwork.Text = "Rotate artwork";
   rotateArtwork.BackColor = Color.Transparent;
   rotateArtwork.AutoSize = true;
   rotateArtwork.Checked = true;
   rotateArtwork.SetBounds(221, 498, 145, 26);
   Controls.Add(rotateArtwork);
   sceneName.SetBounds(450, 449, 470, 36);
   sceneName.BackColor = Color.FromArgb(27, 32, 27);
   sceneName.Padding = new Padding(11, 0, 0, 0);
   sceneName.TextAlign = ContentAlignment.MiddleLeft;
   sceneName.Text = sceneTitles[0];
   Controls.Add(sceneName);
   var nextScene = new Button();
   SetupButton(nextScene, "Next Scene", new Rectangle(930, 449, 150, 36), Color.FromArgb(56, 63, 51));
   nextScene.Anchor = AnchorStyles.Top | AnchorStyles.Right;
   nextScene.Click += delegate {
    AdvanceArtwork();
    if (artworkTimer.Enabled) { artworkTimer.Stop(); artworkTimer.Start(); }
   };
   notes.SetBounds(450,493,630,145);notes.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;
   notes.Multiline=true; notes.ReadOnly=true; notes.ScrollBars=ScrollBars.Vertical; notes.BorderStyle=BorderStyle.FixedSingle;
   notes.BackColor=Color.FromArgb(28,33,29); notes.ForeColor=ForeColor;
   notes.Text = "PATCH HISTORY\r\n\r\nBrowse the project's changes from the original 0.4.0 port onward. Choose your game folder to see the installed build and its latest notes."; Controls.Add(notes);
   status.SetBounds(34,535,336,66);status.BackColor=Color.Transparent;status.Text=preview?"Launcher preview - no game files changed.":"Select a game folder to begin.";Controls.Add(status);
   progress.SetBounds(34,613,336,8);Controls.Add(progress);
   SetupButton(campaign,"PLAY CAMPAIGN",new Rectangle(34,234,336,62),Color.FromArgb(93,104,58));campaign.Name="PlayCampaign";
   SetupButton(multiplayer,"PLAY MULTIPLAYER",new Rectangle(34,308,336,62),Color.FromArgb(142,53,39));multiplayer.Name="PlayMultiplayer";
   SetupButton(repair,"Verify / Repair",new Rectangle(34,444,164,38),Color.FromArgb(51,58,51));
   browse.SetBounds(206,444,164,38);
   var footer = new Label { ForeColor = Color.Silver, AutoSize = true, Anchor = AnchorStyles.Bottom | AnchorStyles.Left }; Controls.Add(footer);
   campaign.Click+=delegate {Launch("CoD3.exe");};multiplayer.Click+=delegate {Launch("CoD3MP.exe");};
   soundtrack.SelectedIndexChanged += delegate {
    ambience.SetEnabled(false);
    ambience.SetEnabled(!preview && soundtrack.SelectedIndex < 2);
    SaveAudioAndArtworkPreferences();
   };
   rotateArtwork.CheckedChanged += delegate {
    artworkTimer.Enabled = rotateArtwork.Checked;
    SaveAudioAndArtworkPreferences();
   };
   artworkTimer.Tick += delegate { if (Visible && WindowState != FormWindowState.Minimized && !ambience.GameRunning) AdvanceArtwork(); };
   checkUpdates.Click += async delegate {
    if (String.IsNullOrEmpty(root)) await ChooseFolder();
    else await Check();
   };
   patchLog.Click += delegate {
    using (var log = new PatchHistory(ReadInstalledNotes(), publishedRelease)) log.ShowDialog(this);
   };
   browse.Click += async delegate { await ChooseFolder(); };
   repair.Click += async delegate { await Check(); };
   FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy){e.Cancel=true;status.Text="Please wait for the current update step to finish before closing.";}};
   VisibleChanged += delegate { artworkTimer.Enabled = Visible && rotateArtwork.Checked; };
   Shown+=async delegate { if(preview) return; LoadPreferences(); if(String.IsNullOrEmpty(root)) await ChooseFolder(); else await Check(); };
   UpdateButtons();
   subtitle.BackColor=Color.Transparent;
   footer.Text="Base game required  |  Launcher V3 (0.3.3)";footer.Font=new Font("Segoe UI",8);footer.Location=new Point(34,710);footer.BackColor=Color.Transparent;
   AddCredit("JARC | Community enhancements\r\nJustin + Justin", new Point(100, 672));
   AddCredit("FrankHUN88453 | Original PC port\r\nCoD3Recomp", new Point(395, 672));
   AddCredit("Treyarch | Original game\r\nIndependent community project", new Point(848, 672));
   notes.Text=notes.Text.Replace("\u0007","|").Replace("•","|");
  }
  Image LoadArtwork(string name){using(var stream=typeof(LauncherForm).Assembly.GetManifestResourceStream(name))if(stream!=null)using(var source=Image.FromStream(stream))return new Bitmap(source);return null;}
  void AddCredit(string text,Point location){Controls.Add(new Label{Text=text,Font=new Font("Segoe UI",8),ForeColor=Color.FromArgb(185,185,169),AutoSize=true,Location=location,BackColor=Color.Transparent,Anchor=AnchorStyles.Bottom|AnchorStyles.Left});}
  protected override void OnPaintBackground(PaintEventArgs e) {
   base.OnPaintBackground(e);
   if(battlefield==null)return;
   float scale=Math.Max(ClientSize.Width/(float)battlefield.Width,ClientSize.Height/(float)battlefield.Height);
   float width=battlefield.Width*scale,height=battlefield.Height*scale;
   e.Graphics.InterpolationMode=InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(battlefield,(ClientSize.Width-width)/2,(ClientSize.Height-height)/2,width,height);
   float unit=DeviceDpi/96f;
   using(var shade=new SolidBrush(Color.FromArgb(238,20,24,21)))e.Graphics.FillRectangle(shade,0,0,404*unit,ClientSize.Height);
   using(var shade=new LinearGradientBrush(new RectangleF(404*unit,0,90*unit,ClientSize.Height),Color.FromArgb(175,20,24,21),Color.Transparent,LinearGradientMode.Horizontal))e.Graphics.FillRectangle(shade,404*unit,0,90*unit,ClientSize.Height);
   using(var shade=new SolidBrush(Color.FromArgb(238,14,18,15)))e.Graphics.FillRectangle(shade,0,ClientSize.Height-88*unit,ClientSize.Width,88*unit);
   if(jarcLogo!=null)e.Graphics.DrawImage(jarcLogo,new RectangleF(34*unit,ClientSize.Height-77*unit,56*unit,56*unit));
   if(treyarchLogo!=null){var plate=new RectangleF(722*unit,ClientSize.Height-66*unit,112*unit,35*unit);using(var ivory=new SolidBrush(Color.FromArgb(205,201,184)))e.Graphics.FillRectangle(ivory,plate);e.Graphics.DrawImage(treyarchLogo,new RectangleF(728*unit,ClientSize.Height-64*unit,100*unit,31*unit));}
  }
  protected override void Dispose(bool disposing) {
   if(disposing){artworkTimer.Dispose();if(ambience!=null)ambience.Dispose();if(sound!=null)sound.Dispose();if(ambienceStream!=null)ambienceStream.Dispose();if(battlefield!=null)battlefield.Dispose();if(jarcLogo!=null)jarcLogo.Dispose();if(treyarchLogo!=null)treyarchLogo.Dispose();if(launchedGame!=null)launchedGame.Dispose();}
   base.Dispose(disposing);
  }
  void AdvanceArtwork() {
   sceneIndex = (sceneIndex + 1) % sceneResources.Length;
   var next = LoadArtwork("Cod3Launcher." + sceneResources[sceneIndex]);
   if (next == null) return;
   if (battlefield != null) battlefield.Dispose();
   battlefield = next;
   sceneName.Text = sceneTitles[sceneIndex];
   Invalidate(true);
  }
  void PlaySoundtrack() {
   StopSoundtrack();
   ambienceStream = typeof(LauncherForm).Assembly.GetManifestResourceStream(soundtrack.SelectedIndex == 0 ? "Cod3Launcher.Frontline" : "Cod3Launcher.Ambience");
   if (ambienceStream == null) return;
   sound = new SoundPlayer(ambienceStream);
   sound.Load();
   sound.PlayLooping();
  }
  void StopSoundtrack() {
   if (sound != null) { sound.Stop(); sound.Dispose(); sound = null; }
   if (ambienceStream != null) { ambienceStream.Dispose(); ambienceStream = null; }
  }
  void SaveAudioAndArtworkPreferences() {
   if (preview || restoringPreferences) return;
   try { SavePreferences(); } catch { status.Text = "Could not save launcher preferences."; }
  }
  string ReadInstalledNotes() {
   if (String.IsNullOrEmpty(root)) return "";
   try {
    string file = SafePaths.Resolve(root, "PATCH-NOTES.txt");
    return File.Exists(file) && new FileInfo(file).Length <= 8 * 1024 * 1024 ? File.ReadAllText(file) : "";
   } catch { return ""; }
  }
  void ShowInstalledNotes() {
   string latest = PatchHistory.Latest(ReadInstalledNotes());
   if (latest.Length > 0)
    notes.Text = ("INSTALLED PATCH NOTES\n\n" + latest).Replace("\r\n", "\n").Replace("\n", "\r\n");
  }
  void SavePreferences() {
   preferences.gameFolder = root;
   preferences.ambientAudio = soundtrack.SelectedIndex < 2;
   preferences.soundtrack = soundtrack.SelectedIndex == 0 ? "frontline" : soundtrack.SelectedIndex == 1 ? "ambience" : "off";
   preferences.rotateArtwork = rotateArtwork.Checked;
   Codec.AtomicText(preferenceFile, Codec.Json().Serialize(preferences));
  }
  void SetupButton(Button b,string text,Rectangle bounds,Color color) {b.Text=text;b.Bounds=bounds;b.BackColor=color;b.ForeColor=Color.White;b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderSize=0;b.Font=new Font("Segoe UI",10,FontStyle.Bold);b.Cursor=Cursors.Hand;Controls.Add(b);}
  void LoadPreferences() {
   try {if(File.Exists(preferenceFile))preferences=Codec.Json().Deserialize<LauncherPreferences>(File.ReadAllText(preferenceFile))??new LauncherPreferences();root=preferences.gameFolder??"";} catch {root="";preferences=new LauncherPreferences();}
   restoringPreferences = true;
   soundtrack.SelectedIndex = !preferences.ambientAudio || preferences.soundtrack == "off" ? 2 : 0;
   rotateArtwork.Checked = preferences.rotateArtwork;
   restoringPreferences = false;
   if(String.IsNullOrEmpty(root) && File.Exists(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"game","codmp_xenonf.xex"))) root=AppDomain.CurrentDomain.BaseDirectory;
   RefreshFolder();
   ShowInstalledNotes();
  }
  async Task ChooseFolder() {
   if(busy)return;
   using(var dialog=new FolderBrowserDialog {Description="Select your existing COD3 PC port folder (contains CoD3.exe, CoD3MP.exe and game)",ShowNewFolderButton=false,SelectedPath=root}) {
    if(dialog.ShowDialog(this)!=DialogResult.OK) return;
    try {Rules.BaseInstall(dialog.SelectedPath);root=dialog.SelectedPath;SavePreferences();RefreshFolder();ShowInstalledNotes();}catch(Exception e){status.Text=e.Message;return;}
   }
   await Check();
  }
  void RefreshFolder() {
   folder.Text = String.IsNullOrEmpty(root) ? "Choose your existing COD3 PC port folder." : root;
   string installed = "-";
   try {
    if (!String.IsNullOrEmpty(root)) {
     string path = SafePaths.Resolve(root, "VERSION.txt");
     if (File.Exists(path)) installed = File.ReadAllText(path).Trim();
    }
   } catch { }
   versions.Text = "Installed build: " + PatchHistory.DisplayVersions(installed) + "\r\nPublished build: " +
    (publishedRelease == null ? "-" : PatchHistory.DisplayVersions(publishedRelease.version));
   UpdateButtons();
  }
  void UpdateButtons() {
   bool valid=false;
   if(!String.IsNullOrEmpty(root))try {Rules.BaseInstall(root);valid=!new Engine(root).NeedsRecovery;}catch{}
   campaign.Enabled=!busy&&!preview&&!launchBlocked&&!ambience.GameRunning&&valid&&File.Exists(Path.Combine(root,"CoD3.exe"));
   multiplayer.Enabled=!busy&&!preview&&!launchBlocked&&!ambience.GameRunning&&valid&&File.Exists(Path.Combine(root,"CoD3MP.exe"));
   browse.Enabled=!busy;repair.Enabled=!busy&&!preview&&!ambience.GameRunning&&!String.IsNullOrEmpty(root);checkUpdates.Enabled=!busy&&!preview&&!ambience.GameRunning;
  }
  async Task Check() {
   if(busy||String.IsNullOrEmpty(root))return;busy=true;launchBlocked=true;UpdateButtons();progress.Value=0;
   Release checkedRelease = null;
   try {
    status.Text="Checking for updates…";
    var reporter=new Progress<UpdateProgress>(p=>{status.Text=p.message;progress.Value=Math.Max(0,Math.Min(100,p.percent));});
    await Task.Run(delegate {
     var engine=new Engine(root);engine.Recover();
     string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");
     if(!File.Exists(key))throw new InvalidOperationException("Update publishing is not set up yet. Your existing installation can still be launched.");
     var transport=new HttpTransport(Service);
     string publicXml=File.ReadAllText(key);string signed=Encoding.UTF8.GetString(transport.Get("stable.json",16*1024*1024));var release=Codec.Verify(signed,publicXml);checkedRelease = release;
     LocalState.Remember(root,signed,publicXml);
     engine.Apply(release,transport,delegate(string message,int percent){((IProgress<UpdateProgress>)reporter).Report(new UpdateProgress {message=message,percent=percent});});
     LocalState.Remember(root,signed,publicXml);
    });
    status.Text="All files verified. Ready to play."; progress.Value=100;
   }catch(WebException e){status.Text=e.Response is HttpWebResponse && ((HttpWebResponse)e.Response).StatusCode==HttpStatusCode.NotFound?"No published release is available yet. You can launch your existing installation.":"Update service is unreachable. Your existing installation is available if no recovery is pending.";}
   catch(Exception e){status.Text=e.Message;}
   if (checkedRelease != null) {
    publishedRelease = checkedRelease;
    notes.Text = ("PUBLISHED BUILD " + PatchHistory.DisplayVersions(checkedRelease.version) + "\n\n" + checkedRelease.notes).Replace("\r\n", "\n").Replace("\n", "\r\n");
   }
   RefreshFolder();
   launchBlocked=!await Task.Run(delegate{try{string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");return LocalState.CanPlay(root,File.Exists(key)?File.ReadAllText(key):null);}catch{return false;}});
   busy=false;UpdateButtons();if(launchBlocked)status.Text+=" Close the game if running, then use Verify / Repair before playing.";
  }
  void Launch(string executable) {
   try {
    if(busy||launchBlocked||String.IsNullOrEmpty(root)||ambience.GameRunning)return;
    ambience.LaunchGame(delegate{new Engine(root).Launch(delegate{
     string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");if(!LocalState.CanPlay(root,File.Exists(key)?File.ReadAllText(key):null))throw new InvalidDataException("Installation needs Verify / Repair before playing.");
     var start=new ProcessStartInfo(SafePaths.Resolve(root,executable)){WorkingDirectory=root,UseShellExecute=false};var process=Process.Start(start);if(process==null)throw new IOException("The game could not be started.");launchedGame=process;
     process.Exited+=delegate{try{if(!IsDisposed&&IsHandleCreated)BeginInvoke(new Action(delegate{if(!IsDisposed&&Object.ReferenceEquals(launchedGame,process)){launchedGame=null;process.Dispose();ambience.GameEnded();UpdateButtons();}}));}catch(InvalidOperationException){}};process.EnableRaisingEvents=true;
    });});
    status.Text="Started "+(executable=="CoD3.exe"?"Campaign":"Multiplayer")+".";UpdateButtons();
   }catch(Exception e){status.Text=e.Message;UpdateButtons();}
  }
  sealed class UpdateProgress {public string message;public int percent;}
 }
 static class Program {
  [STAThread] static int Main(string[] args) {
   Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
   Application.Run(new LauncherForm(Array.IndexOf(args,"--preview")>=0));return 0;
  }
 }
}
