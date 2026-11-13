using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Diagnostics;
using System.Net;
using System.Windows.Forms;

namespace Cod3Launcher {
 public static class Selection {
  public static List<string> Inventory(string root) {
   Rules.BaseInstall(root);
   var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(string path in Rules.RootFiles.Concat(Rules.Archives)) if(File.Exists(SafePaths.Resolve(root,path))) paths.Add(path);
   foreach(string required in new[]{"CoD3.exe","CoD3MP.exe","VERSION.txt","avcodec-62.dll","avutil-60.dll","swresample-6.dll"}.Concat(Rules.Archives)) if(!paths.Contains(required)) throw new FileNotFoundException("Required release file missing: "+required);
   foreach(string pack in Rules.Packs) AddTree(root,"saves/content/00000002/"+pack,paths,true);
   foreach(string custom in new[]{"webley","flamethrower","flare","mosin","mantle"}) AddTree(root,"custom/"+custom,paths,true);
   AddTree(root,"textures/replace",paths,true);
   return paths.OrderBy(p=>p,StringComparer.OrdinalIgnoreCase).ToList();
  }
  static void AddTree(string root,string relative,HashSet<string> paths,bool required) {
   string folder=SafePaths.Resolve(root,relative);
   if(!Directory.Exists(folder)) { if(required) throw new DirectoryNotFoundException("Missing release folder: "+relative); return; }
   int before=paths.Count;
   Walk(root,relative,paths);
   if(required && paths.Count==before) throw new InvalidDataException("Release folder contains no usable files: "+relative);
  }
  static void Walk(string root,string relative,HashSet<string> paths) {
   string folder=SafePaths.Resolve(root,relative);
   foreach(string file in Directory.GetFiles(folder)) {
    string name=Path.GetFileName(file);
    if(name.IndexOf("dump",StringComparison.OrdinalIgnoreCase)>=0 || name.StartsWith("vsconst_",StringComparison.OrdinalIgnoreCase) || name.StartsWith("fit_",StringComparison.OrdinalIgnoreCase) || name.EndsWith(".off",StringComparison.OrdinalIgnoreCase) || name.EndsWith(".bak",StringComparison.OrdinalIgnoreCase)) continue;
    string path=relative+"/"+name;
    SafePaths.Resolve(root,path); if(Rules.Allowed(path)) paths.Add(path);
   }
   foreach(string child in Directory.GetDirectories(folder)) Walk(root,relative+"/"+Path.GetFileName(child),paths);
  }
  public static string Group(string path) {
   foreach(string pack in Rules.Packs) if(path.StartsWith("saves/content/00000002/"+pack+"/",StringComparison.OrdinalIgnoreCase)) return pack;
   if(path.StartsWith("textures/replace/",StringComparison.OrdinalIgnoreCase)) return "Texture replacements (all)";
   if(path.StartsWith("custom/",StringComparison.OrdinalIgnoreCase)) return "Custom weapons and mantle data";
   if(path.StartsWith("game/",StringComparison.OrdinalIgnoreCase)) return "Repacked vehicle and mortar dependencies";
   return "Core executables, runtime and data";
  }
 }
 public static class ReleaseBuilder {
  public static Release Verify(string output,string publicFile) {
   var release=Codec.Verify(File.ReadAllText(Path.Combine(output,"candidate.json")),File.ReadAllText(publicFile));
   foreach(var file in release.files) using(var fullHash=SHA256.Create()) {
    foreach(var chunk in file.chunks) {
     string path=SafePaths.Resolve(output,chunk.key);
     if(!File.Exists(path) || new FileInfo(path).Length!=chunk.packedSize) throw new InvalidDataException("Missing or damaged release object: "+chunk.key);
     using(var input=File.OpenRead(path)) using(var gzip=new GZipStream(input,CompressionMode.Decompress)) using(var hash=SHA256.Create()) {
      byte[] buffer=new byte[65536];int n;long count=0;
      while((n=gzip.Read(buffer,0,buffer.Length))>0) {count+=n;if(count>chunk.size)throw new InvalidDataException("Oversized release chunk");hash.TransformBlock(buffer,0,n,null,0);fullHash.TransformBlock(buffer,0,n,null,0);}
      hash.TransformFinalBlock(new byte[0],0,0);
      if(count!=chunk.size || BitConverter.ToString(hash.Hash).Replace("-","").ToLowerInvariant()!=chunk.sha256)throw new InvalidDataException("Release object hash mismatch");
     }
    }
    fullHash.TransformFinalBlock(new byte[0],0,0);
    if(BitConverter.ToString(fullHash.Hash).Replace("-","").ToLowerInvariant()!=file.sha256)throw new InvalidDataException("Release file hash mismatch: "+file.path);
   }
   return release;
  }
  public static void Keygen(string state,string publicFile) {
   Directory.CreateDirectory(state); string privateFile=Path.Combine(state,"private-key.xml");
   if(File.Exists(publicFile)&&!File.Exists(privateFile))throw new InvalidOperationException("Restore the original publisher private key from your backup. Refusing to replace an established signing identity.");
   if(!File.Exists(privateFile)) using(var rsa=Codec.Rsa()) File.WriteAllText(privateFile,rsa.ToXmlString(true));
   using(var rsa=Codec.Rsa()) { rsa.FromXmlString(File.ReadAllText(privateFile));string publicXml=rsa.ToXmlString(false);
    if(File.Exists(publicFile))using(var existing=Codec.Rsa()){existing.FromXmlString(File.ReadAllText(publicFile));if(existing.ToXmlString(false)!=publicXml)throw new InvalidOperationException("Private key does not match this launcher's established public key.");}
    Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(publicFile))); File.WriteAllText(publicFile,publicXml);
   }
  }
  public static Release Build(string root,string output,string privateFile,Action<string> report) {
   Rules.GameClosed(); root=Path.GetFullPath(root); output=Path.GetFullPath(output);
   if(output.TrimEnd('\\').Equals(root.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase)||output.StartsWith(root.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Build releases outside the live game folder.");
   SafePaths.Resolve(output,"objects/probe");
   var paths=Selection.Inventory(root);
   var snapshots=paths.ToDictionary(p=>p,p=>Stamp(SafePaths.Resolve(root,p)),StringComparer.OrdinalIgnoreCase);
   string version=File.ReadAllText(SafePaths.Resolve(root,"VERSION.txt")).Trim(); Version parsed;
   if(!Version.TryParse(version,out parsed)) throw new InvalidDataException("Invalid VERSION.txt");
   Directory.CreateDirectory(Path.Combine(output,"objects"));
   var release=new Release {version=version};
   string notes=SafePaths.Resolve(root,"PATCH-NOTES.txt");
   if(File.Exists(notes)) { string all=File.ReadAllText(notes); release.notes=all.Substring(0,Math.Min(6000,all.Length)); }
   int index=0;
   foreach(string path in paths) {
    if(report!=null) report((++index)+"/"+paths.Count+" "+path);
    var entry=new ContentFile {path=path};
    using(var source=new FileStream(SafePaths.Resolve(root,path),FileMode.Open,FileAccess.Read,FileShare.Read)) using(var fullHash=SHA256.Create()) {
     byte[] buffer=new byte[Rules.ChunkSize]; int n;
     while((n=ReadChunk(source,buffer))>0) {
      byte[] bytes=new byte[n]; Buffer.BlockCopy(buffer,0,bytes,0,n);
      fullHash.TransformBlock(bytes,0,n,null,0); string hash=Codec.Hash(bytes); byte[] compressed;
      using(var packed=new MemoryStream()) { using(var gzip=new GZipStream(packed,CompressionLevel.Fastest,true)) gzip.Write(bytes,0,n); compressed=packed.ToArray(); }
      string objectHash=Codec.Hash(compressed);string key="objects/"+objectHash+".gz";
      string objectPath=Path.Combine(output,"objects",objectHash+".gz");
      // Rebuild deterministic content, replacing only this builder's object.
      if(!File.Exists(objectPath) || Codec.HashFile(objectPath)!=Codec.Hash(compressed)) File.WriteAllBytes(objectPath,compressed);
      entry.chunks.Add(new Chunk {key=key,sha256=hash,size=n,packedSize=compressed.Length}); entry.size+=n;
     }
     fullHash.TransformFinalBlock(new byte[0],0,0); entry.sha256=BitConverter.ToString(fullHash.Hash).Replace("-","").ToLowerInvariant();
    }
    release.files.Add(entry);
   }
   var after=Selection.Inventory(root);
   if(!paths.SequenceEqual(after,StringComparer.OrdinalIgnoreCase) || snapshots.Any(p=>Stamp(SafePaths.Resolve(root,p.Key))!=p.Value)) throw new IOException("Source files changed during packaging. Candidate was not published; rerun with a stable install.");
   Rules.Validate(release);
   Codec.AtomicText(Path.Combine(output,"candidate.json"),Codec.Sign(release,File.ReadAllText(privateFile)));
   Codec.AtomicText(Path.Combine(output,"inventory.json"),Codec.Json().Serialize(new {version=version,files=paths.Count,bytes=release.files.Sum(f=>f.size),groups=release.files.GroupBy(f=>Selection.Group(f.path)).Select(g=>new {name=g.Key,files=g.Count(),bytes=g.Sum(f=>f.size)}).ToArray()}));
   return release;
  }
  static string Stamp(string file) { var info=new FileInfo(file); return info.Length+":"+info.LastWriteTimeUtc.Ticks; }
  static int ReadChunk(Stream input,byte[] bytes) { int count=0,n; while(count<bytes.Length && (n=input.Read(bytes,count,bytes.Length-count))>0) count+=n; return count; }
 }
 public static class Publisher {
  public static bool RemoteObjectValid(byte[] data,Chunk chunk) { return data.Length==chunk.packedSize&&Codec.Hash(data)==chunk.key.Substring(8,64); }
  static string Quote(string value) { if(value.IndexOfAny(new[]{'"','%','!','\r','\n'})>=0)throw new InvalidDataException("Unsupported character in author-tool argument");return "\""+value+"\""; }
  static void Wrangler(params string[] args) {
   string command="/d /v:off /s /c \"npx.cmd "+String.Join(" ",new[]{"wrangler"}.Concat(args).Select(Quote))+"\"";
   using(var process=Process.Start(new ProcessStartInfo("cmd.exe",command){UseShellExecute=false})) {process.WaitForExit();if(process.ExitCode!=0)throw new IOException("Cloudflare command failed. Current release was not deliberately advanced; inspect the output above.");}
  }
  static bool RemoteObjectExists(Chunk chunk) {
   try {return RemoteObjectValid(new HttpTransport("https://cod3-updates.hovgtv.workers.dev").Get(chunk.key,Rules.ChunkSize+65536),chunk);}
   catch(WebException e) {if(e.Response is HttpWebResponse && ((HttpWebResponse)e.Response).StatusCode==HttpStatusCode.NotFound){e.Response.Dispose();return false;}throw;}
  }
  static void Publish(string output,string publicFile,string config) {
   string candidate=Path.Combine(output,"candidate.json");var release=ReleaseBuilder.Verify(output,publicFile);string manifestHash=Codec.HashFile(candidate);
   var chunks=release.files.SelectMany(f=>f.chunks).GroupBy(c=>c.key).Select(g=>g.First()).ToList();int index=0;
   foreach(var chunk in chunks) {
    Console.WriteLine("Content "+(++index)+"/"+chunks.Count);
    if(!RemoteObjectExists(chunk)) {
     Wrangler("r2","object","put","cod3-port-updates/"+chunk.key,"--file",SafePaths.Resolve(output,chunk.key),"--content-type","application/octet-stream","--remote","--config",config);
     if(!RemoteObjectExists(chunk))throw new IOException("Uploaded object could not be confirmed; public release was not changed.");
    }
   }
   if(Codec.HashFile(candidate)!=manifestHash)throw new IOException("Candidate changed during upload; public release was not changed.");
   Wrangler("r2","object","put","cod3-port-updates/releases/"+release.version+"-"+manifestHash+".json","--file",candidate,"--content-type","application/json","--remote","--config",config);
   Wrangler("r2","object","put","cod3-port-updates/stable.json","--file",candidate,"--content-type","application/json","--remote","--config",config);
   Console.WriteLine("Published "+release.version+". Verify the launcher on a second PC before announcing the release.");
  }
  static void Prepare(string kitRoot) {
   kitRoot=Path.GetFullPath(kitRoot);string root="";string hint=Path.Combine(kitRoot,"GAME-FOLDER.txt");if(File.Exists(hint))root=File.ReadAllText(hint).Trim();
   if(!Directory.Exists(root))using(var picker=new FolderBrowserDialog{Description="Select your stable COD3 PC port installation",ShowNewFolderButton=false}) {if(picker.ShowDialog()!=DialogResult.OK)throw new InvalidOperationException("Cancelled; no release prepared.");root=picker.SelectedPath;}
   string state=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"HOVGtv","CoD3Publisher");
   string publicFile=Path.Combine(kitRoot,"player","public-key.xml");ReleaseBuilder.Keygen(state,publicFile);
   ReleaseBuilder.Build(root,Path.Combine(kitRoot,"release-candidate"),Path.Combine(state,"private-key.xml"),Console.WriteLine);
   Console.WriteLine("Candidate prepared. Review release-candidate/inventory.json and test before running PUBLISH-RELEASE.cmd.");
   Console.WriteLine("Signing key stays in your local application data. Keep a private backup; do not send it to players.");
  }
  [STAThread]
  public static int Main(string[] args) {
   try {
    if(args.Length==2&&args[0]=="prepare"){Prepare(args[1]);return 0;}
    if(args.Length==4&&args[0]=="publish"){Publish(args[1],args[2],args[3]);return 0;}
    if(args.Length==3 && args[0]=="keygen") { ReleaseBuilder.Keygen(args[1],args[2]); Console.WriteLine("Publisher key ready. Keep private-key.xml private and backed up."); return 0; }
    if(args.Length==3 && args[0]=="inventory") {
     string root=Path.GetFullPath(args[1]); var paths=Selection.Inventory(root);
     var result=new {version=File.ReadAllText(SafePaths.Resolve(root,"VERSION.txt")).Trim(),files=paths.Count,bytes=paths.Sum(p=>new FileInfo(SafePaths.Resolve(root,p)).Length),groups=paths.GroupBy(Selection.Group).Select(g=>new {name=g.Key,files=g.Count(),bytes=g.Sum(p=>new FileInfo(SafePaths.Resolve(root,p)).Length)}).ToArray(),paths=paths};
     File.WriteAllText(args[2],Codec.Json().Serialize(result)); Console.WriteLine("Inventory ready: "+paths.Count+" files. No payload uploaded."); return 0;
    }
    if(args.Length==3 && args[0]=="verify") {var release=ReleaseBuilder.Verify(args[1],args[2]);Console.WriteLine("Verified candidate "+release.version+": signature and all content hashes passed.");return 0;}
    if(args.Length==4 && args[0]=="build") {var release=ReleaseBuilder.Build(args[1],args[2],args[3],Console.WriteLine);Console.WriteLine("Candidate "+release.version+" ready. Nothing uploaded or published."); return 0;}
    Console.Error.WriteLine("Usage: Publisher keygen STATE_DIR PUBLIC_XML | inventory GAME_DIR OUTPUT_JSON | build GAME_DIR OUTPUT_DIR PRIVATE_XML"); return 2;
   } catch(Exception e) {Console.Error.WriteLine("FAILED: "+e.Message);return 1;}
  }
 }
}
