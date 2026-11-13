using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Cod3Launcher;

sealed class MemoryTransport : ITransport {
 public Dictionary<string,byte[]> objects = new Dictionary<string,byte[]>(); public int calls; public int failAt;
 public byte[] Get(string key,int max) { calls++; if(calls==failAt) throw new IOException("Simulated interrupted download"); var value=objects[key]; if(value.Length>max) throw new InvalidDataException("Oversized transport response"); return value; }
}
sealed class Fixture {
 public string root = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-data",Guid.NewGuid().ToString("N").Substring(0,12));
 public MemoryTransport transport = new MemoryTransport();
 public Fixture() { Put("game/codmp_xenonf.xex","base mp"); Put("game/default.xex","base sp"); Put("CoD3MP.exe","original exe");Put("CoD3.exe","original campaign"); Put("CoD3Recomp.ini","player_name = Justin\r\nsystem_link_peers = keep-me\r\n# My settings\r\nmaster_server = old\r\n"); }
 public string PathOf(string path) { return SafePaths.Resolve(root,path); }
 public void Put(string path,string text) { string p=PathOf(path); Directory.CreateDirectory(Path.GetDirectoryName(p)); File.WriteAllText(p,text,new UTF8Encoding(false)); }
 public string Read(string path) { return File.ReadAllText(PathOf(path)); }
 public Release Release(params string[] paths) {
  var release=new Release {version="0.5.867",notes="Fixture release"};
  foreach(string path in paths) {
   byte[] data=Encoding.UTF8.GetBytes("updated: "+path);
   var chunks=new List<Chunk>(); string hash=Codec.Hash(data); byte[] packed;
   using(var output=new MemoryStream()) { using(var gz=new GZipStream(output,CompressionMode.Compress,true)) gz.Write(data,0,data.Length); packed=output.ToArray(); }
   var chunk=new Chunk {key="objects/"+hash+".gz",sha256=hash,size=data.Length,packedSize=packed.Length};
   chunks.Add(chunk); transport.objects[chunk.key]=packed;
   release.files.Add(new ContentFile {path=path,size=data.Length,sha256=hash,chunks=chunks});
  }
  return release;
 }
}

class Tests {
 static int passed, failed;
 static void Test(string name,Action action) { try { action(); Console.WriteLine("PASS: "+name); passed++; } catch(Exception e) { Console.WriteLine("FAIL: "+name+" - "+e); failed++; } }
 static void Equal<T>(T expected,T actual) { if(!Object.Equals(expected,actual)) throw new Exception("Expected "+expected+", got "+actual); }
 static void True(bool value) { if(!value) throw new Exception("Expected true"); }
 static void Reject(Action action) { try { action(); } catch { return; } throw new Exception("Expected rejection"); }
 static int Main() {
  Test("valid content path",delegate { var f=new Fixture(); Equal(Path.Combine(f.root,"textures","replace","menu.png"),f.PathOf("textures/replace/menu.png")); });
  Test("Windows traversal and alias paths rejected",delegate { var f=new Fixture(); foreach(string path in new[]{"../outside.exe","/absolute","C:/absolute","textures\\replace\\x.png","textures/replace/x.png:stream","textures/replace/../x.png","textures/replace/CON.png","textures/replace/x. ","textures//x.png","textures/replace/COM1.txt"}) Reject(delegate { f.PathOf(path); }); });
  Test("manifest cannot overwrite saves or player configuration",delegate { var f=new Fixture(); foreach(string path in new[]{"CoD3Recomp.ini","saves/player.dat","game/default.xex","DEPLOY.cmd"}) { var release=f.Release(path); Reject(delegate { Rules.Validate(release); }); } });
  Test("duplicate Windows paths rejected",delegate {var f=new Fixture(); var r=f.Release("CoD3MP.exe","cod3mp.exe"); Reject(delegate {Rules.Validate(r);}); });
  Test("chunk bounds and external download keys rejected",delegate { var f=new Fixture(); var r=f.Release("CoD3MP.exe"); r.files[0].chunks[0].key="https://other.invalid/file"; Reject(delegate { Rules.Validate(r); }); r=f.Release("CoD3MP.exe"); r.files[0].chunks[0].size=Rules.ChunkSize+1; Reject(delegate { Rules.Validate(r); }); });
  Test("signed release verifies and tampering fails",delegate {var f=new Fixture(); using(var key=Codec.Rsa()) { string signed=Codec.Sign(f.Release("CoD3MP.exe"),key.ToXmlString(true)); Equal("0.5.867",Codec.Verify(signed,key.ToXmlString(false)).version); var e=Codec.Json().Deserialize<Envelope>(signed); e.payload=Convert.ToBase64String(Encoding.UTF8.GetBytes("tampered")); Reject(delegate {Codec.Verify(Codec.Json().Serialize(e),key.ToXmlString(false));}); using(var other=Codec.Rsa()) Reject(delegate {Codec.Verify(signed,other.ToXmlString(false));}); } });
  Test("missing base game rejected",delegate {string root=Path.Combine(Path.GetTempPath(),Guid.NewGuid().ToString("N")); Directory.CreateDirectory(root); Reject(delegate {new Engine(root);}); });
  Test("install and repeat skip unchanged downloads",delegate {var f=new Fixture();var r=f.Release("CoD3MP.exe","textures/replace/menu.png");var engine=new Engine(f.root);engine.Apply(r,f.transport,null);Equal("updated: CoD3MP.exe",f.Read("CoD3MP.exe"));Equal("updated: textures/replace/menu.png",f.Read("textures/replace/menu.png"));Equal(2,f.transport.calls);engine.Apply(r,f.transport,null);Equal(2,f.transport.calls);True(!engine.NeedsRecovery); });
  Test("settings, saves and base archives survive",delegate {var f=new Fixture();f.Put("saves/player.dat","SAVE");var r=f.Release("CoD3MP.exe");new Engine(f.root).Apply(r,f.transport,null);True(f.Read("CoD3Recomp.ini").Contains("player_name = Justin"));True(f.Read("CoD3Recomp.ini").Contains("system_link_peers = keep-me"));True(f.Read("CoD3Recomp.ini").Contains("# My settings"));True(f.Read("CoD3Recomp.ini").Contains("master_server = "+Rules.Master));Equal("SAVE",f.Read("saves/player.dat"));Equal("base sp",f.Read("game/default.xex")); });
  Test("repair uses verified cache for corrupt installed file",delegate {var f=new Fixture();var r=f.Release("textures/replace/menu.png");var e=new Engine(f.root);e.Apply(r,f.transport,null);int calls=f.transport.calls;f.Put("textures/replace/menu.png","broken");e.Apply(r,f.transport,null);Equal("updated: textures/replace/menu.png",f.Read("textures/replace/menu.png"));Equal(calls,f.transport.calls); });
  Test("damaged content does not touch original installation",delegate {var f=new Fixture();var r=f.Release("CoD3MP.exe");var c=r.files[0].chunks[0];var data=f.transport.objects[c.key];data[0]=0;Reject(delegate {new Engine(f.root).Apply(r,f.transport,null);});Equal("original exe",f.Read("CoD3MP.exe"));True(!new Engine(f.root).NeedsRecovery); });
  Test("valid gzip with wrong hash rejected",delegate {var f=new Fixture();var r=f.Release("CoD3MP.exe");r.files[0].chunks[0].sha256=new string('a',64);string previous=r.files[0].chunks[0].key;r.files[0].chunks[0].key="objects/"+new string('a',64)+".gz";f.transport.objects[r.files[0].chunks[0].key]=f.transport.objects[previous];Reject(delegate {new Engine(f.root).Apply(r,f.transport,null);});Equal("original exe",f.Read("CoD3MP.exe")); });
  Test("download interruption reuses completed chunks on retry",delegate {var f=new Fixture();var r=f.Release("CoD3MP.exe","textures/replace/menu.png");f.transport.failAt=2;var e=new Engine(f.root);Reject(delegate {e.Apply(r,f.transport,null);});Equal("original exe",f.Read("CoD3MP.exe"));f.transport.failAt=0;e.Apply(r,f.transport,null);Equal(3,f.transport.calls);Equal("updated: CoD3MP.exe",f.Read("CoD3MP.exe")); });
  Test("commit failure rolls back original and newly added files",delegate {var f=new Fixture();var r=f.Release("textures/replace/new.png","CoD3MP.exe");var e=new Engine(f.root);Reject(delegate {e.Apply(r,f.transport,delegate(string text,int p){if(text=="Installing CoD3MP.exe") throw new IOException("Simulated disk failure");});});Equal("original exe",f.Read("CoD3MP.exe"));True(!File.Exists(f.PathOf("textures/replace/new.png")));True(!e.NeedsRecovery); });
  Test("restart recovers a partially installed transaction",delegate {var f=new Fixture();string id=Guid.NewGuid().ToString("N");f.Put(".cod3-launcher/backups/"+id+"/CoD3MP.exe","original exe");f.Put("CoD3MP.exe","partial");f.Put("textures/replace/new.png","partial new file");var j=new Journal {backup=id,operations=new List<Operation>{new Operation{path="CoD3MP.exe",existed=true,size=12,sha256=Codec.Hash(Encoding.UTF8.GetBytes("original exe"))},new Operation{path="textures/replace/new.png",existed=false}}};f.Put(".cod3-launcher/journal.json",Codec.Json().Serialize(j));var e=new Engine(f.root);True(e.NeedsRecovery);e.Recover();Equal("original exe",f.Read("CoD3MP.exe"));True(!File.Exists(f.PathOf("textures/replace/new.png")));True(!e.NeedsRecovery); });
  Test("concurrent updater cannot start",delegate {var f=new Fixture();var r=f.Release("CoD3MP.exe");Directory.CreateDirectory(f.PathOf(".cod3-launcher/cache"));using(var held=new FileStream(f.PathOf(".cod3-launcher/update.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) Reject(delegate {new Engine(f.root).Apply(r,f.transport,null);});Equal(0,f.transport.calls); });
  Test("newer development build cannot be downgraded",delegate {var f=new Fixture();f.Put("VERSION.txt","0.5.999");Reject(delegate {new Engine(f.root).Apply(f.Release("CoD3MP.exe"),f.transport,null);});Equal("original exe",f.Read("CoD3MP.exe"));Equal(0,f.transport.calls); });
  Test("settings migration is idempotent",delegate {string ini="player_name = Justin\nMASTER_SERVER = old\nmaster_server = duplicate\n";string once=Settings.Merge(ini);Equal(once,Settings.Merge(once));True(!once.Contains("old"));True(!once.Contains("duplicate"));True(once.Contains("player_name = Justin")); });
  Test("publisher includes all six packs and campaign textures without dumps",delegate {
   var f=FullFixture();var paths=Selection.Inventory(f.root);
   foreach(string pack in Rules.Packs)True(paths.Contains("saves/content/00000002/"+pack+"/mp/map.cod"));
   True(paths.Contains("textures/replace/maps/campaign.png"));True(paths.Contains("custom/flamethrower/flame_arms.bin"));
   True(!paths.Any(p=>p.Contains("dump")));True(!paths.Contains("game/default.xex"));True(!paths.Contains("CoD3Recomp.ini"));
  });
  Test("publisher refuses a missing DLC pack",delegate {var f=FullFixture();Directory.Move(f.PathOf("saves/content/00000002/Champs"),f.PathOf("saves/content/00000002/Champs-off"));Reject(delegate {Selection.Inventory(f.root);});});
  Test("publisher signs and roundtrips an entire real file release",delegate {
   var f=FullFixture();using(var large=File.Create(f.PathOf("textures/replace/large.dds"))){byte[] block=new byte[1024*1024];for(int i=0;i<9;i++)large.Write(block,0,block.Length);}
   string output=Path.Combine(f.root+"-candidate");string state=f.root+"-keys";string publicFile=Path.Combine(state,"public-key.xml");
   ReleaseBuilder.Keygen(state,publicFile);var r=ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),null);
   Equal("0.5.867",ReleaseBuilder.Verify(output,publicFile).version);True(!File.Exists(Path.Combine(output,"private-key.xml")));Equal(2,r.files.Single(file=>file.path=="textures/replace/large.dds").chunks.Count);
   string before=f.Read("CoD3MP.exe");var destination=new Fixture();var transport=new DiskTransport(output);new Engine(destination.root).Apply(r,transport,null);Equal(before,destination.Read("CoD3MP.exe"));Equal("campaign texture",destination.Read("textures/replace/maps/campaign.png"));
   string objectFile=Path.Combine(output,r.files[0].chunks[0].key.Replace('/',Path.DirectorySeparatorChar));File.WriteAllText(objectFile,"corrupt");Reject(delegate {ReleaseBuilder.Verify(output,publicFile);});
  });
  Test("same-length corrupted recovery backup keeps journal and blocks restore",delegate {
   var f=new Fixture();string id=Guid.NewGuid().ToString("N");f.Put(".cod3-launcher/backups/"+id+"/CoD3MP.exe","BAD BACKUP!!");f.Put("CoD3MP.exe","partial");
   var journal=new Journal {backup=id,operations=new List<Operation>{new Operation{path="CoD3MP.exe",existed=true,size=12,sha256=Codec.Hash(Encoding.UTF8.GetBytes("original exe"))}}};
   f.Put(".cod3-launcher/journal.json",Codec.Json().Serialize(journal));var engine=new Engine(f.root);Reject(delegate{engine.Recover();});True(engine.NeedsRecovery);Equal("partial",f.Read("CoD3MP.exe"));
  });
  Test("missing signing state cannot silently replace an established key",delegate {
   var f=new Fixture();string state=f.root+"-key-state";string publicFile=Path.Combine(f.root,"public-key.xml");ReleaseBuilder.Keygen(state,publicFile);string before=File.ReadAllText(publicFile);File.Delete(Path.Combine(state,"private-key.xml"));Reject(delegate{ReleaseBuilder.Keygen(state,publicFile);});Equal(before,File.ReadAllText(publicFile));
  });
  Test("mismatched signing state cannot overwrite an established key",delegate {
   var f=new Fixture();string state=f.root+"-key-state";string pub=Path.Combine(f.root,"public-key.xml");ReleaseBuilder.Keygen(state,pub);string before=File.ReadAllText(pub);using(var other=Codec.Rsa())File.WriteAllText(Path.Combine(state,"private-key.xml"),other.ToXmlString(true));Reject(delegate{ReleaseBuilder.Keygen(state,pub);});Equal(before,File.ReadAllText(pub));
  });
  Test("publisher rejects source as output before writing objects",delegate {var f=FullFixture();string state=f.root+"-keys";string pub=Path.Combine(state,"public.xml");ReleaseBuilder.Keygen(state,pub);Reject(delegate{ReleaseBuilder.Build(f.root,f.root,Path.Combine(state,"private-key.xml"),null);});True(!Directory.Exists(f.PathOf("objects")));});
  Test("publisher refuses linked output before writing any objects",delegate {var f=FullFixture();string link=Path.Combine(Path.GetPathRoot(Environment.SystemDirectory),"Users","All Users","cod3-must-not-create-"+Guid.NewGuid().ToString("N"));bool rejected=false;try{ReleaseBuilder.Build(f.root,link,"unused-key",null);}catch(InvalidDataException e){rejected=e.Message.StartsWith("Linked folders/files");}True(rejected);True(!Directory.Exists(link));True(!Directory.Exists(f.PathOf("objects")));});
  Test("same-length remote corruption fails content-address verification",delegate {byte[] correct=Encoding.UTF8.GetBytes("correct");var chunk=new Chunk {key="objects/"+Codec.Hash(correct)+".gz",packedSize=7};True(Publisher.RemoteObjectValid(correct,chunk));True(!Publisher.RemoteObjectValid(Encoding.UTF8.GetBytes("corrupt"),chunk));});
  Test("launch callback runs under the update lock",delegate {var f=new Fixture();var engine=new Engine(f.root);bool called=false;engine.Launch(delegate{called=true;Reject(delegate{new Engine(f.root).Apply(f.Release("CoD3MP.exe"),f.transport,null);});});True(called);Equal(0,f.transport.calls);});
  Test("known damaged install stays blocked after failed repair",delegate {var f=new Fixture();f.Put("VERSION.txt","0.5.867");var release=f.Release("CoD3MP.exe");using(var key=Codec.Rsa()){string pub=key.ToXmlString(false);string signed=Codec.Sign(release,key.ToXmlString(true));LocalState.Remember(f.root,signed,pub);True(!LocalState.CanPlay(f.root,pub));f.transport.failAt=1;Reject(delegate{new Engine(f.root).Apply(release,f.transport,null);});True(!LocalState.CanPlay(f.root,pub));f.transport.failAt=0;new Engine(f.root).Apply(release,f.transport,null);True(LocalState.CanPlay(f.root,pub));f.Put("CoD3MP.exe","corrupt after closing launcher");True(!LocalState.CanPlay(f.root,pub));}});
  Test("second build reuses unchanged files and repairs a damaged cached object",delegate {
   var f=FullFixture();string output=f.root+"-incremental";string state=f.root+"-state";string pub=Path.Combine(state,"public.xml");ReleaseBuilder.Keygen(state,pub);
   var first=ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),null);var messages=new List<string>();
   var second=ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),messages.Add);
   Equal(first.files.Count,second.files.Count);Equal(first.files.Count,messages.Count(m=>m.StartsWith("Reused: ")));True(messages.Any(m=>m.StartsWith("Build summary:")));
   var texture=second.files.Single(p=>p.path=="textures/replace/maps/campaign.png");File.WriteAllText(SafePaths.Resolve(output,texture.chunks[0].key),"damaged cache");
   messages.Clear();ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),messages.Add);True(messages.Contains("Built: textures/replace/maps/campaign.png"));ReleaseBuilder.Verify(output,pub);
  });
  Test("changed source is rebuilt despite unchanged size and timestamp",delegate {
   var f=FullFixture();string output=f.root+"-incremental";string state=f.root+"-state";string pub=Path.Combine(state,"public.xml");ReleaseBuilder.Keygen(state,pub);var one=ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),null);
   string path=f.PathOf("textures/replace/maps/campaign.png");DateTime stamp=File.GetLastWriteTimeUtc(path);f.Put("textures/replace/maps/campaign.png","CAMPAIGN TEXTURE");File.SetLastWriteTimeUtc(path,stamp);var messages=new List<string>();var two=ReleaseBuilder.Build(f.root,output,Path.Combine(state,"private-key.xml"),messages.Add);
   True(one.files.Single(p=>p.path.EndsWith("campaign.png")).sha256!=two.files.Single(p=>p.path.EndsWith("campaign.png")).sha256);True(messages.Contains("Built: textures/replace/maps/campaign.png"));True(messages.Count(m=>m.StartsWith("Reused: "))==two.files.Count-1);
  });
  Test("cloud metadata must match exact size and R2 ETag",delegate {
   var chunk=new Chunk {packedSize=42};string md5=new string('a',32);
   True(Publisher.MetadataMatches(42,"\""+md5+"\"",chunk,md5));True(!Publisher.MetadataMatches(41,"\""+md5+"\"",chunk,md5));True(!Publisher.MetadataMatches(42,"\""+new string('b',32)+"\"",chunk,md5));True(!Publisher.MetadataMatches(42,"W/\""+md5+"\"",chunk,md5));True(!Publisher.MetadataMatches(42,"\""+md5+"-2\"",chunk,md5));True(!Publisher.MetadataMatches(42,null,chunk,md5));
  });
  Test("a new client update skips matching files and fetches only its added file",delegate {
   var f=new Fixture();var first=f.Release("CoD3MP.exe","textures/replace/menu.png");var engine=new Engine(f.root);engine.Apply(first,f.transport,null);int before=f.transport.calls;
   var next=f.Release("CoD3MP.exe","textures/replace/menu.png","textures/replace/new.png");next.version="0.5.868";engine.Apply(next,f.transport,null);Equal(before+1,f.transport.calls);Equal("updated: textures/replace/menu.png",f.Read("textures/replace/menu.png"));Equal("updated: textures/replace/new.png",f.Read("textures/replace/new.png"));
  });
  Test("publisher binds metadata checksum to the immutable compressed object key",delegate {
   byte[] bytes=Encoding.UTF8.GetBytes("correct");var c=new Chunk {key="objects/"+Codec.Hash(bytes)+".gz",packedSize=bytes.Length};
   using(var correct=new MemoryStream(bytes)){string expected=Publisher.ValidatedMd5(correct,c);True(expected.Length==32);}
   using(var changed=new MemoryStream(Encoding.UTF8.GetBytes("corrupt")))Reject(delegate{Publisher.ValidatedMd5(changed,c);});
  });
  Test("metadata fast path skips body while unusual ETags use SHA256 fallback",delegate {
   byte[] bytes=Encoding.UTF8.GetBytes("correct");var c=new Chunk {key="objects/"+Codec.Hash(bytes)+".gz",packedSize=bytes.Length};string md5;
   using(var input=new MemoryStream(bytes))md5=Publisher.ValidatedMd5(input,c);
   var cloud=new FakeCloud {body=bytes,metadata=new CloudMetadata {size=bytes.Length,etag="\""+md5+"\""}};
   True(Publisher.HasObject(c,md5,cloud));Equal(0,cloud.gets);
   foreach(string etag in new[]{null,"W/\""+md5+"\"","\""+md5+"-2\"","\""+new string('b',32)+"\""}){cloud.metadata.etag=etag;cloud.gets=0;True(Publisher.HasObject(c,md5,cloud));Equal(1,cloud.gets);}
   cloud.body=Encoding.UTF8.GetBytes("corrupt");True(!Publisher.HasObject(c,md5,cloud));cloud.metadata=null;cloud.gets=0;True(!Publisher.HasObject(c,md5,cloud));Equal(0,cloud.gets);
  });
  Test("publisher holds local upload object against modification",delegate {var f=new Fixture();string path=f.PathOf("object.gz");File.WriteAllText(path,"correct");var c=new Chunk {key="objects/"+Codec.Hash(File.ReadAllBytes(path))+".gz",packedSize=7};using(var held=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){Publisher.ValidatedMd5(held,c);Reject(delegate{File.WriteAllText(path,"corrupt");});Equal("correct",File.ReadAllText(path));}});
  Console.WriteLine("RESULT: "+passed+" passed, "+failed+" failed"); return failed==0?0:1;
 }
 static Fixture FullFixture() {
  var f=new Fixture();foreach(string file in Rules.RootFiles.Concat(Rules.Archives))f.Put(file,"fixture: "+file);
  f.Put("VERSION.txt","0.5.867");foreach(string pack in Rules.Packs)f.Put("saves/content/00000002/"+pack+"/mp/map.cod","map payload");
  foreach(string custom in new[]{"webley","flamethrower","flare","mosin","mantle"})f.Put("custom/"+custom+"/asset.bin","asset");
  f.Put("custom/flamethrower/flame_arms.bin","arms");f.Put("custom/webley/fit_dump.bin","debug");f.Put("textures/replace/maps/campaign.png","campaign texture");f.Put("textures/dump/raw.png","dump");return f;
 }
}
sealed class DiskTransport : ITransport {
 readonly string folder;public DiskTransport(string directory){folder=directory;}
 public byte[] Get(string key,int max){byte[] data=File.ReadAllBytes(SafePaths.Resolve(folder,key));if(data.Length>max)throw new InvalidDataException("Too much data");return data;}
}
sealed class FakeCloud : ICloudObjects {
 public CloudMetadata metadata;public byte[] body;public int gets;
 public CloudMetadata Head(string key){return metadata;}
 public byte[] Get(string key,int max){gets++;if(body.Length>max)throw new InvalidDataException("Too large");return body;}
}
