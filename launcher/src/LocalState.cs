using System;
using System.IO;
namespace Cod3Launcher {
 public static class LocalState {
  public static void Remember(string root,string signed,string publicKey) {
   var release=Codec.Verify(signed,publicKey);string version=SafePaths.Resolve(root,"VERSION.txt");
   if(File.Exists(version)&&File.ReadAllText(version).Trim()==release.version)Codec.AtomicText(SafePaths.Resolve(root,".cod3-launcher/expected-release.json"),signed);
  }
  public static bool CanPlay(string root,string publicKey) {
   try {
    Rules.BaseInstall(root);if(new Engine(root).NeedsRecovery||!File.Exists(SafePaths.Resolve(root,"CoD3MP.exe"))||!File.Exists(SafePaths.Resolve(root,"CoD3.exe")))return false;
    string expected=SafePaths.Resolve(root,".cod3-launcher/expected-release.json");
    if(!File.Exists(expected))return true; // Existing legacy install has never been managed by this launcher.
    if(String.IsNullOrEmpty(publicKey))return false;
    var release=Codec.Verify(File.ReadAllText(expected),publicKey);
    Version local,offered;string version=SafePaths.Resolve(root,"VERSION.txt");
    if(File.Exists(version)&&Version.TryParse(File.ReadAllText(version).Trim(),out local)&&Version.TryParse(release.version,out offered)&&local>offered)return true;
    return new Engine(root).Changes(release,null).Count==0;
   }catch{return false;}
  }
 }
}
