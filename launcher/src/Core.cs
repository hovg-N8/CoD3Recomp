using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace Cod3Launcher {
 public sealed class Chunk { public string key; public string sha256; public int size; public int packedSize; }
 public sealed class ContentFile { public string path; public long size; public string sha256; public List<Chunk> chunks = new List<Chunk>(); }
 public sealed class Release {
  public int schema = 1; public string version; public string notes = "";
  public List<ContentFile> files = new List<ContentFile>();
 }
 public sealed class Envelope { public string payload; public string signature; }
 public sealed class Operation { public string path; public bool existed; public long size; public string sha256; }
 public sealed class Journal { public string backup; public List<Operation> operations = new List<Operation>(); }

 public static class Codec {
  public static JavaScriptSerializer Json() { return new JavaScriptSerializer { MaxJsonLength = 16 * 1024 * 1024, RecursionLimit = 30 }; }
  public static string Hash(byte[] bytes) { using (var h = SHA256.Create()) return Hex(h.ComputeHash(bytes)); }
  public static string HashFile(string path) { using (var h = SHA256.Create()) using(var f = File.OpenRead(path)) return Hex(h.ComputeHash(f)); }
  static string Hex(byte[] bytes) { return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant(); }
  public static string Sign(Release release, string privateXml) {
   var bytes = Encoding.UTF8.GetBytes(Json().Serialize(release));
   using (var rsa = Rsa()) { rsa.FromXmlString(privateXml);
    return Json().Serialize(new Envelope { payload = Convert.ToBase64String(bytes), signature = Convert.ToBase64String(rsa.SignData(bytes, CryptoConfig.MapNameToOID("SHA256"))) });
   }
  }
  public static Release Verify(string signed, string publicXml) {
   var envelope = Json().Deserialize<Envelope>(signed);
   if (envelope == null || String.IsNullOrEmpty(envelope.payload) || String.IsNullOrEmpty(envelope.signature)) throw new InvalidDataException("This release is not signed.");
   byte[] bytes = Convert.FromBase64String(envelope.payload);
   using (var rsa = Rsa()) { rsa.FromXmlString(publicXml);
    if (!rsa.VerifyData(bytes, CryptoConfig.MapNameToOID("SHA256"), Convert.FromBase64String(envelope.signature))) throw new InvalidDataException("Release signature is invalid. No files were installed.");
   }
   var release = Json().Deserialize<Release>(Encoding.UTF8.GetString(bytes));
   Rules.Validate(release); return release;
  }
  public static RSACryptoServiceProvider Rsa() { var rsa = new RSACryptoServiceProvider(3072); rsa.PersistKeyInCsp = false; return rsa; }
  public static void AtomicText(string path, string value) {
   Directory.CreateDirectory(Path.GetDirectoryName(path));
   string tmp = path + ".new-" + Guid.NewGuid().ToString("N"); File.WriteAllText(tmp, value, new UTF8Encoding(false));
   if (File.Exists(path)) File.Replace(tmp, path, null); else File.Move(tmp, path);
  }
 }

 public static class SafePaths {
  public static string Resolve(string root, string relative) {
   if (String.IsNullOrWhiteSpace(relative) || relative.Contains("\\") || relative.Contains(":") || relative.StartsWith("/") || relative.Length > 220) throw new InvalidDataException("Invalid relative content path.");
   var parts = relative.Split('/');
   foreach(string part in parts) {
    if (part.Length == 0 || part == "." || part == ".." || part.EndsWith(".") || part.EndsWith(" ") || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)", RegexOptions.IgnoreCase)) throw new InvalidDataException("Unsafe content path: " + relative);
   }
   string basePath = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
   string result = Path.GetFullPath(Path.Combine(basePath, Path.Combine(parts)));
   if (!result.StartsWith(basePath, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Content path escaped the installation.");
   string current = result;
   while (!String.IsNullOrEmpty(current)) {
    try { if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw new InvalidDataException("Linked folders/files cannot be updated: " + current); }
    catch(FileNotFoundException) {} catch(DirectoryNotFoundException) {}
    string parent = Path.GetDirectoryName(current); if (parent == current) break; current = parent;
   }
   return result;
  }
 }

 public static class Rules {
  public const int ChunkSize = 8 * 1024 * 1024;
  public const string Master = "https://cod3-master.hovgtv.workers.dev";
  public static readonly string[] Packs = { "Bravo Map Pack", "Champs", "Valor Map Pack", "Carentan", "Seelow", "SeelowNight" };
  public static readonly string[] RootFiles = { "CoD3.exe", "CoD3MP.exe", "VERSION.txt", "PATCH-NOTES.txt", "avcodec-62.dll", "avutil-60.dll", "swresample-6.dll", "ffmpeg-LICENSE.txt", "vehicles.txt", "bot_kits.txt", "world_models.txt", "dom_flags.txt", "ctf-flags.txt", "ctf-routes.txt" };
  public static readonly string[] Archives = { "game/mp/mp_merv/mp_merv_vehicles.cod", "game/mp/mp_merv/mp_merv_vehiclefx.cod", "game/sp/vehfx/mp_merv_vehiclefx.cod", "game/sp/saint_lo/vehicles_panzer.cod", "game/sp/crssrds/vehicles_stuka.cod", "game/sp/laison/laison_mortar.cod", "game/sp/laison/laison_mortar_fx.cod", "game/sp/laison/laison_mortar_zn08.cod" };
  static readonly Regex Digest = new Regex("^[a-f0-9]{64}$");
  public static bool Allowed(string path) {
   if (RootFiles.Contains(path, StringComparer.OrdinalIgnoreCase) || Archives.Contains(path, StringComparer.OrdinalIgnoreCase)) return true;
   if (path.StartsWith("textures/replace/", StringComparison.OrdinalIgnoreCase)) return new[] { ".png", ".dds", ".tga", ".jpg", ".jpeg", ".txt" }.Contains(Path.GetExtension(path).ToLowerInvariant());
   if (path.StartsWith("custom/", StringComparison.OrdinalIgnoreCase)) {
    var p = path.Split('/'); return p.Length >= 3 && new[] { "webley", "flamethrower", "flare", "mosin", "mantle" }.Contains(p[1].ToLowerInvariant()) && new[] { ".png", ".mesh", ".bin", ".wav", ".txt" }.Contains(Path.GetExtension(path).ToLowerInvariant());
   }
   return Packs.Any(pack => path.StartsWith("saves/content/00000002/" + pack + "/", StringComparison.OrdinalIgnoreCase));
  }
  public static void Validate(Release release) {
   Version parsed;
   if (release == null || release.schema != 1 || !Version.TryParse(release.version, out parsed) || release.files == null || release.files.Count == 0 || release.files.Count > 30000) throw new InvalidDataException("Unsupported or empty release manifest.");
   long total = 0; var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var file in release.files) {
    SafePaths.Resolve(Path.GetTempPath(), file.path);
    if (!Allowed(file.path) || !seen.Add(file.path) || file.size < 0 || file.size > 8L * 1024 * 1024 * 1024 || !Digest.IsMatch(file.sha256 ?? "") || file.chunks == null || file.chunks.Count > 1024) throw new InvalidDataException("Invalid file entry: " + file.path);
    long size = 0;
    foreach(var chunk in file.chunks) {
     if (chunk.size <= 0 || chunk.size > ChunkSize || chunk.packedSize <= 0 || chunk.packedSize > ChunkSize + 65536 || !Digest.IsMatch(chunk.sha256 ?? "") || !Regex.IsMatch(chunk.key ?? "", "^objects/[a-f0-9]{64}\\.gz$")) throw new InvalidDataException("Invalid download chunk.");
     size += chunk.size;
    }
    if (size != file.size) throw new InvalidDataException("File size and chunk sizes do not match.");
    total += file.size;
   }
   if (total > 128L * 1024 * 1024 * 1024) throw new InvalidDataException("Release is too large.");
  }
  public static void BaseInstall(string root) {
   if (!File.Exists(SafePaths.Resolve(root, "game/codmp_xenonf.xex")) || !File.Exists(SafePaths.Resolve(root, "game/default.xex"))) throw new InvalidDataException("Select an extracted COD3 PC port folder containing the campaign and multiplayer game files.");
  }
  public static void GameClosed() {
   foreach(string name in new[] { "CoD3", "CoD3MP" }) if (Process.GetProcessesByName(name).Length > 0) throw new IOException("Close Campaign and Multiplayer before updating or changing launcher settings.");
  }
 }

 public interface ITransport { byte[] Get(string key, int maxBytes); }
 public sealed class HttpTransport : ITransport {
  readonly Uri root;
  public HttpTransport(string url) {
   root = new Uri(url.TrimEnd('/') + "/");
   if (root.Scheme != "https" || !String.IsNullOrEmpty(root.UserInfo)) throw new InvalidDataException("Update service must use HTTPS.");
   ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
  }
  public byte[] Get(string key, int maxBytes) {
   if (key != "stable.json" && !Regex.IsMatch(key, "^objects/[a-f0-9]{64}\\.gz$")) throw new InvalidDataException("Invalid service request.");
   var request = (HttpWebRequest)WebRequest.Create(new Uri(root, key));
   request.Timeout = 30000; request.ReadWriteTimeout = 30000; request.AllowAutoRedirect = false; request.UserAgent = "CoD3Launcher/0.1.0";
   using(var response = (HttpWebResponse)request.GetResponse()) {
    if (response.StatusCode != HttpStatusCode.OK || response.ContentLength > maxBytes) throw new InvalidDataException("Unexpected update response.");
    using(var stream = response.GetResponseStream()) using(var data = new MemoryStream()) {
     var buffer = new byte[65536]; int n;
     while ((n = stream.Read(buffer, 0, buffer.Length)) > 0) { if (data.Length + n > maxBytes) throw new InvalidDataException("Download exceeded its declared limit."); data.Write(buffer, 0, n); }
     return data.ToArray();
    }
   }
  }
 }

 public sealed class Engine {
  public void Launch(Action start) { Rules.GameClosed(); using(Lock()) { Rules.GameClosed(); RecoverLocked(); PrepareSettings(); start(); } }
  readonly string root;
  public Engine(string directory) { root = Path.GetFullPath(directory); Rules.BaseInstall(root); }
  string Managed(string path) { return SafePaths.Resolve(root, ".cod3-launcher/" + path); }
  FileStream Lock() { Directory.CreateDirectory(Managed("cache")); return new FileStream(Managed("update.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None); }
  public bool NeedsRecovery { get { return File.Exists(Managed("journal.json")); } }
  public List<ContentFile> Changes(Release release, Action<string,int> progress) {
   Rules.Validate(release); var changed = new List<ContentFile>(); int i = 0;
   foreach(var file in release.files) {
    if (progress != null) progress("Checking " + file.path, ++i * 100 / release.files.Count);
    string target = SafePaths.Resolve(root, file.path);
    if (!Matches(target, file.size, file.sha256)) changed.Add(file);
   }
   return changed;
  }
  public static bool Matches(string path, long size, string hash) { return File.Exists(path) && new FileInfo(path).Length == size && Codec.HashFile(path) == hash; }
  public void Recover() { Rules.GameClosed(); using(Lock()) RecoverLocked(); }
  void RecoverLocked() {
   if (!NeedsRecovery) return;
   var journal = Codec.Json().Deserialize<Journal>(File.ReadAllText(Managed("journal.json")));
   if (journal == null || journal.operations == null || !Regex.IsMatch(journal.backup ?? "", "^[0-9a-f]{32}$")) throw new InvalidDataException("Recovery journal is damaged. Keep the backup folder and contact the release author.");
   var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var operation in journal.operations) {
    if(operation == null || !seen.Add(operation.path))throw new InvalidDataException("Invalid recovery operations.");
    if (operation.path != "CoD3Recomp.ini" && !Rules.Allowed(operation.path)) throw new InvalidDataException("Invalid recovery path.");
    SafePaths.Resolve(root,operation.path);
    if(operation.existed && !Matches(Managed("backups/"+journal.backup+"/"+operation.path),operation.size,operation.sha256)) throw new IOException("Recovery backup failed verification: "+operation.path+". Keep the backup and close the launcher.");
   }
   foreach(var operation in journal.operations) {
    string target = SafePaths.Resolve(root, operation.path);
    string backup = Managed("backups/" + journal.backup + "/" + operation.path);
    if (operation.existed) {
     if (!File.Exists(backup)) throw new IOException("An update backup is missing: " + operation.path);
     Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(backup, target, true);
    } else if (File.Exists(target)) File.Delete(target);
   }
   foreach(var operation in journal.operations) {
    string target=SafePaths.Resolve(root,operation.path);
    if(operation.existed ? !Matches(target,operation.size,operation.sha256) : File.Exists(target))throw new IOException("Recovery could not be verified: "+operation.path);
   }
   File.Delete(Managed("journal.json"));
  }
  public void Apply(Release release, ITransport transport, Action<string,int> progress) {
   Rules.Validate(release); Rules.GameClosed();
   using(Lock()) {
    Rules.GameClosed();
    RecoverLocked();
    Version installed, offered;
    string versionPath = SafePaths.Resolve(root, "VERSION.txt");
    if (File.Exists(versionPath) && Version.TryParse(File.ReadAllText(versionPath).Trim(), out installed) && Version.TryParse(release.version, out offered) && installed > offered) throw new InvalidDataException("Your local build is newer than the published release. It was not downgraded.");
    var changed = Changes(release, progress);
    string oldIni = File.Exists(SafePaths.Resolve(root,"CoD3Recomp.ini")) ? File.ReadAllText(SafePaths.Resolve(root,"CoD3Recomp.ini")) : "";
    string newIni = Settings.Merge(oldIni);
    long required = changed.Sum(f => f.size) * 3 + 64L * 1024 * 1024;
    if (new DriveInfo(Path.GetPathRoot(root)).AvailableFreeSpace < required) throw new IOException("Not enough disk space for the update and its recovery backup.");
    string run = Guid.NewGuid().ToString("N");
    string stage = Managed("staging/" + run);
    Directory.CreateDirectory(stage);
    int done = 0; int totalChunks = Math.Max(1, changed.Sum(f => f.chunks.Count));
    try {
    foreach(var file in changed) {
     string staged = SafePaths.Resolve(stage, file.path); Directory.CreateDirectory(Path.GetDirectoryName(staged));
     using(var destination = File.Create(staged)) foreach(var chunk in file.chunks) {
      string cached = Managed("cache/" + chunk.sha256 + ".bin");
      if (!Matches(cached, chunk.size, chunk.sha256)) {
       if (progress != null) progress("Downloading " + file.path, done * 100 / totalChunks);
       byte[] packed = transport.Get(chunk.key, chunk.packedSize);
       if (packed.Length != chunk.packedSize) throw new InvalidDataException("Incomplete download; reopen the launcher to retry.");
       string partial = cached + ".part";
       using(var input = new MemoryStream(packed)) using(var gzip = new GZipStream(input, CompressionMode.Decompress)) using(var output = File.Create(partial)) {
        byte[] buffer = new byte[65536]; int n; long count = 0;
        while ((n = gzip.Read(buffer,0,buffer.Length)) > 0) { count += n; if (count > chunk.size) throw new InvalidDataException("Download expands beyond its declared size."); output.Write(buffer,0,n); }
        if (count != chunk.size) throw new InvalidDataException("Incomplete content chunk.");
       }
       if (!Matches(partial, chunk.size, chunk.sha256)) { File.Delete(partial); throw new InvalidDataException("Downloaded content failed verification. No game files changed."); }
       if (File.Exists(cached)) File.Delete(cached); File.Move(partial,cached);
      }
      using(var input = File.OpenRead(cached)) input.CopyTo(destination);
      if (progress != null) progress("Preparing " + file.path, ++done * 100 / totalChunks);
     }
     if (!Matches(staged, file.size, file.sha256)) throw new InvalidDataException("Prepared file failed verification: " + file.path);
    }
    var paths = changed.Select(f => f.path).ToList();
    if (newIni != oldIni) { File.WriteAllText(SafePaths.Resolve(stage,"CoD3Recomp.ini"),newIni,new UTF8Encoding(false)); paths.Add("CoD3Recomp.ini"); }
    if (paths.Count == 0) { if(progress != null) progress("All files verified. Ready to play.",100); return; }
    var journal = new Journal { backup = run };
    foreach(string path in paths) {
     string target = SafePaths.Resolve(root,path); bool exists = File.Exists(target);
     var operation=new Operation {path=path,existed=exists};
     if (exists) { operation.size=new FileInfo(target).Length;operation.sha256=Codec.HashFile(target);string backup = Managed("backups/" + run + "/" + path); Directory.CreateDirectory(Path.GetDirectoryName(backup)); File.Copy(target,backup,false);if(!Matches(backup,operation.size,operation.sha256))throw new IOException("Could not verify backup: "+path); }
     journal.operations.Add(operation);
    }
    Rules.GameClosed();
    Codec.AtomicText(Managed("journal.json"),Codec.Json().Serialize(journal));
    try {
     foreach(string path in paths) {
      if (progress != null) progress("Installing " + path,100);
      string target = SafePaths.Resolve(root,path); Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(SafePaths.Resolve(stage,path),target,true);
     }
     foreach(var file in changed) if (!Matches(SafePaths.Resolve(root,file.path),file.size,file.sha256)) throw new IOException("Installed file failed verification: " + file.path);
     File.Delete(Managed("journal.json"));
    } catch { RecoverLocked(); throw; }
    if (progress != null) progress("Update complete. Ready to play.",100);
    } finally {
     // Delete only this run's explicitly named staging files, including partial transfers.
     foreach(string path in changed.Select(f=>f.path).Concat(new[]{"CoD3Recomp.ini"})) {
      string p=SafePaths.Resolve(stage,path);try {if(File.Exists(p))File.Delete(p);}catch(IOException){}catch(UnauthorizedAccessException){}
     }
    }
   }
  }
  void PrepareSettings() {
    string ini = SafePaths.Resolve(root,"CoD3Recomp.ini");
    string before = File.Exists(ini) ? File.ReadAllText(ini) : ""; string after = Settings.MergeMaster(before);
    if (before != after) { if(File.Exists(ini)) { string backup=Managed("backups/"+Guid.NewGuid().ToString("N")+"/CoD3Recomp.ini"); Directory.CreateDirectory(Path.GetDirectoryName(backup)); File.Copy(ini,backup); } Codec.AtomicText(ini,after); }
  }
 }

 public static class Settings {
  public static string MergeMaster(string current) {
   const string pattern=@"(?m)^[ \t]*master_server[ \t]*=[^\r\n]*";
   if(Regex.IsMatch(current,pattern,RegexOptions.IgnoreCase))return Regex.Replace(current,pattern,"master_server = "+Rules.Master,RegexOptions.IgnoreCase);
   return current+(current.Length>0&&!current.EndsWith("\n")?"\r\n":"")+"master_server = "+Rules.Master+"\r\n";
  }
  public static string Merge(string current) {
   var values = new Dictionary<string,string> {
    {"master_server",Rules.Master},{"campaign_guns","1"},{"big_memory","1"},{"campaign_sounds","1"},{"campaign_gunshots","0"},
    {"campaign_icons","1"},{"campaign_shot_sounds","1"},{"ads_in_air","1"},{"quick_sprint","1"},{"quick_landing","1"},
    {"bounces","1"},{"faster_ads","1"},{"aim_walk","80"},{"launcher","1"},{"four_secondary","1"},{"two_grenades","1"},
    {"ability_recharge","10"},{"extra_map_memory","0"},{"native_commands","0"},{"mg34_no_deploy","1"},{"sniper_walk","30"},{"sniper_sway","50"},{"bot_vehicles","1"},{"melee_lunge","1"}
   };
   string result = current;
   foreach(var item in values) {
    string pattern = @"(?m)^[ \t]*" + Regex.Escape(item.Key) + @"[ \t]*=[^\r\n]*";
    if(Regex.IsMatch(result,pattern,RegexOptions.IgnoreCase)) result=Regex.Replace(result,pattern,item.Key+" = "+item.Value,RegexOptions.IgnoreCase);
    else { if(result.Length>0 && !result.EndsWith("\n")) result += "\r\n"; result += item.Key+" = "+item.Value+"\r\n"; }
   }
   return result;
  }
 }
}
