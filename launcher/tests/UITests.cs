using System;
using System.Linq;
using System.Windows.Forms;
using Cod3Launcher;
class UITests {
 [STAThread] static int Main() {
  Application.EnableVisualStyles();
  using(var form=new LauncherForm(true)) {
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="PLAY CAMPAIGN")!=1)throw new Exception("Campaign button missing");
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="PLAY MULTIPLAYER")!=1)throw new Exception("Multiplayer button missing");
   if(form.Controls.OfType<Button>().Count(b=>b.Text=="Verify / Repair")!=1)throw new Exception("Repair button missing");
   if(form.Controls.OfType<Button>().Any(b=>b.Name.StartsWith("Play")&&b.Enabled))throw new Exception("Preview must not launch a game");
   form.CreateControl();
  }
  Console.WriteLine("PASS: launcher form constructs; campaign, multiplayer and repair controls; preview blocks launching.");return 0;
 }
}
