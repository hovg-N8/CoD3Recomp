using System;
using Cod3Launcher;
class AudioTests {
 static void Equal(int expected,int value){if(expected!=value)throw new Exception("Expected "+expected+", got "+value);}
 static int Main(){
  int play=0,stop=0;var controller=new AmbienceController(delegate{play++;},delegate{stop++;});
  controller.SetEnabled(false);Equal(0,play);controller.SetEnabled(true);Equal(1,play);controller.SetEnabled(true);Equal(1,play);
  controller.Suspend();Equal(1,stop);controller.SetEnabled(true);Equal(1,play);controller.Resume();Equal(2,play);
  controller.SetEnabled(false);Equal(2,stop);controller.Suspend();controller.Resume();Equal(2,play);
  controller.SetEnabled(true);Equal(3,play);controller.Dispose();Equal(3,stop);controller.Resume();controller.SetEnabled(true);Equal(3,play);
  int starts=0;play=stop=0;var running=new AmbienceController(delegate{play++;},delegate{stop++;});running.SetEnabled(true);
  if(!running.LaunchGame(delegate{starts++;}))throw new Exception("First launch rejected");Equal(1,play);Equal(1,stop);
  if(running.LaunchGame(delegate{starts++;}))throw new Exception("Second launch accepted during active game");Equal(1,starts);Equal(1,play);
  running.SetEnabled(false);running.SetEnabled(true);Equal(1,play);running.GameEnded();Equal(2,play);
  try{running.LaunchGame(delegate{throw new InvalidOperationException("Simulated failed launch");});throw new Exception("Expected launch failure");}catch(InvalidOperationException){}
  Equal(3,play);running.Dispose();
  Console.WriteLine("PASS: optional audio, idempotent toggles, pause/resume, mute retention, failed/repeated game launches and disposal.");return 0;
 }
}
