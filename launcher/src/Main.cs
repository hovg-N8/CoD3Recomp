using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Cod3Launcher {
 public sealed class LauncherPreferences { public string gameFolder = ""; }
 public sealed class LauncherForm : Form {
  const string Service = "https://cod3-updates.hovgtv.workers.dev";
  readonly Label status = new Label(); readonly Label versions = new Label(); readonly Label folder = new Label();
  readonly TextBox notes = new TextBox(); readonly ProgressBar progress = new ProgressBar();
  readonly Button campaign = new Button(); readonly Button multiplayer = new Button(); readonly Button repair = new Button(); readonly Button browse = new Button();
  readonly bool preview; string root = ""; bool busy; bool launchBlocked=true;
  readonly string preferenceFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"HOVGtv","CoD3Launcher","settings.json");
  public LauncherForm(bool previewMode) {
   preview=previewMode; Text="Call of Duty 3 | PC Launcher"; Name="CoD3Launcher";
   ClientSize=new Size(900,620); MinimumSize=new Size(820,650); StartPosition=FormStartPosition.CenterScreen;
   BackColor=Color.FromArgb(19,22,21); ForeColor=Color.FromArgb(232,235,225); Font=new Font("Segoe UI",10); AutoScaleMode=AutoScaleMode.Dpi;
   var title=new Label {Text="CALL OF DUTY 3",Font=new Font("Segoe UI",30,FontStyle.Bold),AutoSize=true,Location=new Point(30,24)};
   var subtitle=new Label {Text="PC PORT  /  COMMUNITY LAUNCHER",Font=new Font("Segoe UI",10,FontStyle.Bold),ForeColor=Color.FromArgb(181,192,135),AutoSize=true,Location=new Point(34,83)};
   Controls.Add(title); Controls.Add(subtitle);
   versions.SetBounds(34,121,820,24); versions.Text="Installed build: —     Published build: —"; versions.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right; Controls.Add(versions);
   folder.SetBounds(34,153,680,38); folder.AutoEllipsis=true; folder.ForeColor=Color.Silver; folder.Text="Choose your existing COD3 PC port folder."; Controls.Add(folder);
   SetupButton(browse,"Choose folder",new Rectangle(730,149,135,35),Color.FromArgb(51,58,51));browse.Anchor=AnchorStyles.Top|AnchorStyles.Right;
   notes.SetBounds(34,204,831,193); notes.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;
   notes.Multiline=true; notes.ReadOnly=true; notes.ScrollBars=ScrollBars.Vertical; notes.BorderStyle=BorderStyle.FixedSingle;
   notes.BackColor=Color.FromArgb(28,33,29); notes.ForeColor=ForeColor;
   notes.Text="YOUR COD3 COLLECTION\r\n\r\nChamps • Bravo • Valor\r\nCarentan • Seelow • Seelow Night\r\n\r\nAdded weapons, vehicle support, and updated menu, campaign and UI textures.\r\nUpdates are checked automatically when the launcher opens."; Controls.Add(notes);
   status.SetBounds(34,415,831,44); status.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;status.Text=preview?"Launcher preview — no game files changed.":"Select a game folder to begin.";Controls.Add(status);
   progress.SetBounds(34,467,831,12);progress.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(progress);
   SetupButton(campaign,"PLAY CAMPAIGN",new Rectangle(34,501,295,62),Color.FromArgb(93,104,58));campaign.Name="PlayCampaign";
   SetupButton(multiplayer,"PLAY MULTIPLAYER",new Rectangle(344,501,295,62),Color.FromArgb(142,53,39));multiplayer.Name="PlayMultiplayer";
   SetupButton(repair,"Verify / Repair",new Rectangle(654,501,211,62),Color.FromArgb(51,58,51));repair.Anchor=AnchorStyles.Top|AnchorStyles.Right;
   var footer=new Label {Text="Community project • Your own installed base game is required • Launcher 0.1.0",ForeColor=Color.Gray,AutoSize=true,Location=new Point(34,587)};Controls.Add(footer);
   campaign.Click+=delegate {Launch("CoD3.exe");};multiplayer.Click+=delegate {Launch("CoD3MP.exe");};
   browse.Click+=async delegate { await ChooseFolder(); };repair.Click+=async delegate { await Check(); };
   FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy){e.Cancel=true;status.Text="Please wait for the current update step to finish before closing.";}};
   Shown+=async delegate { if(preview) {campaign.Enabled=false;multiplayer.Enabled=false;return;} LoadPreferences(); if(String.IsNullOrEmpty(root)) await ChooseFolder(); else await Check(); };
   UpdateButtons();
  }
  void SetupButton(Button b,string text,Rectangle bounds,Color color) {b.Text=text;b.Bounds=bounds;b.BackColor=color;b.ForeColor=Color.White;b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderSize=0;b.Font=new Font("Segoe UI",11,FontStyle.Bold);b.Cursor=Cursors.Hand;Controls.Add(b);}
  void LoadPreferences() {
   try {if(File.Exists(preferenceFile)) root=Codec.Json().Deserialize<LauncherPreferences>(File.ReadAllText(preferenceFile)).gameFolder??"";} catch {root="";}
   if(String.IsNullOrEmpty(root) && File.Exists(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"game","codmp_xenonf.xex"))) root=AppDomain.CurrentDomain.BaseDirectory;
   RefreshFolder();
  }
  async Task ChooseFolder() {
   if(busy)return;
   using(var dialog=new FolderBrowserDialog {Description="Select your existing COD3 PC port folder (contains CoD3.exe, CoD3MP.exe and game)",ShowNewFolderButton=false,SelectedPath=root}) {
    if(dialog.ShowDialog(this)!=DialogResult.OK) return;
    try {Rules.BaseInstall(dialog.SelectedPath);root=dialog.SelectedPath;Codec.AtomicText(preferenceFile,Codec.Json().Serialize(new LauncherPreferences {gameFolder=root}));RefreshFolder();}catch(Exception e){status.Text=e.Message;return;}
   }
   await Check();
  }
  void RefreshFolder() {folder.Text=String.IsNullOrEmpty(root)?"Choose your existing COD3 PC port folder.":root;string version="unknown";try {string p=SafePaths.Resolve(root,"VERSION.txt");if(File.Exists(p))version=File.ReadAllText(p).Trim();}catch{}versions.Text="Installed build: "+version+"     Published build: —";UpdateButtons();}
  void UpdateButtons() {
   bool valid=false;
   if(!String.IsNullOrEmpty(root))try {Rules.BaseInstall(root);valid=!new Engine(root).NeedsRecovery;}catch{}
   campaign.Enabled=!busy&&!preview&&!launchBlocked&&valid&&File.Exists(Path.Combine(root,"CoD3.exe"));
   multiplayer.Enabled=!busy&&!preview&&!launchBlocked&&valid&&File.Exists(Path.Combine(root,"CoD3MP.exe"));
   browse.Enabled=!busy;repair.Enabled=!busy&&!preview&&!String.IsNullOrEmpty(root);
  }
  async Task Check() {
   if(busy||String.IsNullOrEmpty(root))return;busy=true;launchBlocked=true;UpdateButtons();progress.Value=0;
   try {
    status.Text="Checking for updates…";
    var reporter=new Progress<UpdateProgress>(p=>{status.Text=p.message;progress.Value=Math.Max(0,Math.Min(100,p.percent));});
    Release result=await Task.Run(delegate {
     var engine=new Engine(root);engine.Recover();
     string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");
     if(!File.Exists(key))throw new InvalidOperationException("Update publishing is not set up yet. Your existing installation can still be launched.");
     var transport=new HttpTransport(Service);
     string publicXml=File.ReadAllText(key);string signed=Encoding.UTF8.GetString(transport.Get("stable.json",16*1024*1024));var release=Codec.Verify(signed,publicXml);
     LocalState.Remember(root,signed,publicXml);
     engine.Apply(release,transport,delegate(string message,int percent){((IProgress<UpdateProgress>)reporter).Report(new UpdateProgress {message=message,percent=percent});});
     LocalState.Remember(root,signed,publicXml);
     return release;
    });
    RefreshFolder(); versions.Text=versions.Text.Replace("Published build: —","Published build: "+result.version);notes.Text="RELEASE "+result.version+"\r\n\r\n"+result.notes;status.Text="All files verified. Ready to play.";progress.Value=100;
   }catch(WebException e){status.Text=e.Response is HttpWebResponse && ((HttpWebResponse)e.Response).StatusCode==HttpStatusCode.NotFound?"No published release is available yet. You can launch your existing installation.":"Update service is unreachable. Your existing installation is available if no recovery is pending.";}
   catch(Exception e){status.Text=e.Message;}
   launchBlocked=!await Task.Run(delegate{try{string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");return LocalState.CanPlay(root,File.Exists(key)?File.ReadAllText(key):null);}catch{return false;}});
   busy=false;UpdateButtons();if(launchBlocked)status.Text+=" Close the game if running, then use Verify / Repair before playing.";
  }
  void Launch(string executable) {
   try {if(busy||launchBlocked||String.IsNullOrEmpty(root))return;new Engine(root).Launch(delegate{string key=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"public-key.xml");if(!LocalState.CanPlay(root,File.Exists(key)?File.ReadAllText(key):null))throw new InvalidDataException("Installation needs Verify / Repair before playing.");var start=new ProcessStartInfo(SafePaths.Resolve(root,executable)){WorkingDirectory=root,UseShellExecute=false};Process.Start(start);});status.Text="Started "+(executable=="CoD3.exe"?"Campaign":"Multiplayer")+".";}
   catch(Exception e){status.Text=e.Message;UpdateButtons();}
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
