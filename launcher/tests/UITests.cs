using System;
using System.Linq;
using System.Drawing;
using System.IO;
using System.Media;
using System.Windows.Forms;
using Cod3Launcher;
class UITests {
 [STAThread] static int Main(string[] args) {
  Application.EnableVisualStyles();
  using(var form=new LauncherForm(true)) {
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="PLAY CAMPAIGN")!=1)throw new Exception("Campaign button missing");
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="PLAY MULTIPLAYER")!=1)throw new Exception("Multiplayer button missing");
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="Verify / Repair")!=1)throw new Exception("Repair button missing");
   if(form.Controls.OfType<Button>().Any(b=>b.Name.StartsWith("Play")&&b.Enabled))throw new Exception("Preview must not launch a game");
   if(form.Controls.OfType<ComboBox>().Single(c=>c.Name=="Soundtrack").Items.Count!=3)throw new Exception("Soundtrack choices missing");
   if(!form.Controls.OfType<Button>().Any(b=>b.Text=="Check for Updates")||!form.Controls.OfType<Button>().Any(b=>b.Text=="Patch Log"))throw new Exception("Update or patch log control missing");
   using(var history=new PatchHistory("## 0.5.1005: A later patch\nNew notes.",new Release{version="0.5.1006",notes="Published notes."})) {
    var controls=history.Controls[0].Controls;
    var list=controls.OfType<ListBox>().Single();
    if(list.Items.Count<900||!list.Items.Cast<object>().Any(i=>i.ToString().StartsWith("0.4.0")))throw new Exception("Full patch history missing");
    if(PatchHistory.DisplayVersions("0.6.4")!="0.6.04" || PatchHistory.DisplayVersions("0.6.40")!="0.6.40") throw new Exception("Build display format is incorrect");
    controls.OfType<TextBox>().Single(b=>!b.ReadOnly).Text="0.6.4";
    if(!list.Items.Cast<object>().Any(i=>i.ToString().StartsWith("0.6.04:")))throw new Exception("Renumbered patch history missing");
    controls.OfType<TextBox>().Single(b=>!b.ReadOnly).Text="0.5.1005";
    if(list.Items.Count!=1||!controls.OfType<TextBox>().Single(b=>b.ReadOnly).Text.Contains("New notes."))throw new Exception("Patch search failed");
   }
   using(var stream=typeof(LauncherForm).Assembly.GetManifestResourceStream("Cod3Launcher.Battlefield"))using(var image=Image.FromStream(stream)){if(image.Width<1000||image.Height<600)throw new Exception("Battlefield artwork missing or too small");}
   using(var stream=typeof(LauncherForm).Assembly.GetManifestResourceStream("Cod3Launcher.Ambience"))using(var player=new SoundPlayer(stream)){player.Load();}
   using(var stream=typeof(LauncherForm).Assembly.GetManifestResourceStream("Cod3Launcher.Frontline"))
   using(var player=new SoundPlayer(stream)){player.Load();}
   form.StartPosition=FormStartPosition.Manual;
   form.Location=new Point(-20000,-20000);
   form.ShowInTaskbar=false;
   form.Show();
   Application.DoEvents();
   var next=form.Controls.OfType<Button>().Single(b=>b.Text=="Next Scene");
   foreach(string scene in new[]{"Saint-Lô in the rain","Falaise armored column","Chambois crossing","Polish armor at dawn","Railway yard at dawn","Normandy farm at dusk","Quarry field post","Village under fire","Typhoons over Normandy","Orchard convoy at night"}) {
    next.PerformClick();
    if(!form.Controls.OfType<Label>().Any(l=>l.Text==scene)) throw new Exception("Artwork rotation did not reach "+scene);
   }
   // Shorten the existing timer for an actual automatic advance in preview.
   var timer=(Timer)typeof(LauncherForm).GetField("artworkTimer",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(form);
   var sceneLabel=form.Controls.OfType<Label>().Single(l=>l.Text=="Orchard convoy at night");
   bool advanced=false;
   sceneLabel.TextChanged+=delegate { advanced=true; timer.Stop(); };
   timer.Stop();
   timer.Interval=250;
   timer.Start();
   var deadline=DateTime.UtcNow.AddSeconds(3);
   while(DateTime.UtcNow<deadline && !advanced) {
    Application.DoEvents();
    System.Threading.Thread.Sleep(5);
   }
   timer.Stop();
   if(!advanced || sceneLabel.Text!="Saint-Lô in the rain")throw new Exception("Automatic artwork timer did not advance");
   if(args.Length==2&&args[0]=="--render"){form.Refresh();using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,new Rectangle(0,0,form.Width,form.Height));bitmap.Save(args[1],System.Drawing.Imaging.ImageFormat.Png);}}
   form.Close();
  }
  Console.WriteLine("PASS: launcher controls, audio toggle, embedded artwork/audio, and safe preview.");return 0;
 }
}
