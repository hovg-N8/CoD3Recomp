using System;
namespace Cod3Launcher {
 public sealed class AmbienceController : IDisposable {
  readonly Action play,stop;bool enabled,suspended,playing,disposed;
  public AmbienceController(Action start,Action end) {play=start;stop=end;}
  public void SetEnabled(bool value) {enabled=value;Refresh();}
  public void Suspend() {suspended=true;Refresh();}
  public void Resume() {suspended=false;Refresh();}
  public bool GameRunning {get;private set;}
  public bool LaunchGame(Action start){if(disposed||GameRunning)return false;Suspend();try{start();GameRunning=true;return true;}catch{Resume();throw;}}
  public void GameEnded(){GameRunning=false;Resume();}
  void Refresh() {
   bool wanted=enabled&&!suspended&&!disposed;
   if(wanted==playing)return;
   if(wanted){try{play();playing=true;}catch{playing=false;}}
   else{try{stop();}catch{}playing=false;}
  }
  public void Dispose() {disposed=true;Refresh();}
 }
}
