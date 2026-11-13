using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace Cod3Launcher {
 public sealed class PatchHistory : Form {
  sealed class Entry {
   public string Title, Body;
   public override string ToString() { return Title; }
  }

  readonly List<Entry> entries = new List<Entry>();
  readonly ListBox versions = new ListBox();
  readonly TextBox body = new TextBox();
  static readonly Regex Headings = new Regex(@"(?m)^##[ \t]+([^\r\n]+)");

  internal static string DisplayVersions(string text) {
   return Regex.Replace(text, @"(?<![\d.])0\.6\.(\d)(?![\d.])", "0.6.0$1");
  }

  internal static string Latest(string document) {
   if (String.IsNullOrWhiteSpace(document)) return "";
   var headings = Headings.Matches(document);
   if (headings.Count == 0) return document.Trim();
   int end = headings.Count > 1 ? headings[1].Index : document.Length;
   return DisplayVersions(document.Substring(headings[0].Index + 3, end - headings[0].Index - 3).Trim());
  }

  public PatchHistory(string installedNotes, Release published) {
   Text = "CoD3 | Patch log";
   StartPosition = FormStartPosition.CenterParent;
   ClientSize = new Size(940, 600);
   MinimumSize = new Size(730, 450);
   BackColor = Color.FromArgb(23, 27, 24);
   ForeColor = Color.FromArgb(239, 235, 216);
   Font = new Font("Segoe UI", 10);
   using (var stream = typeof(PatchHistory).Assembly.GetManifestResourceStream("Cod3Launcher.PatchHistory"))
   using (var reader = new StreamReader(stream)) AddDocument(reader.ReadToEnd(), "Project history", false);
   AddDocument(installedNotes, "Installed patch notes", true);
   if (published != null && !String.IsNullOrWhiteSpace(published.notes))
    entries.Insert(0, new Entry { Title = "Published build " + DisplayVersions(published.version), Body = published.notes });

   var search = new TextBox { Dock = DockStyle.Fill, AccessibleName = "Search patch versions and notes" };
   var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(14), RowCount = 2, ColumnCount = 2 };
   layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 315));
   layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
   layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
   layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
   var label = new Label { Text = "Search versions or patch notes", Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft };
   versions.Dock = DockStyle.Fill;
   versions.IntegralHeight = false;
   versions.HorizontalScrollbar = true;
   versions.BackColor = Color.FromArgb(37, 43, 38);
   versions.ForeColor = ForeColor;
   body.Dock = DockStyle.Fill;
   body.Multiline = true;
   body.ReadOnly = true;
   body.ScrollBars = ScrollBars.Vertical;
   body.BackColor = Color.FromArgb(29, 34, 30);
   body.ForeColor = ForeColor;
   body.AccessibleName = "Selected patch notes";
   layout.Controls.Add(label, 0, 0);
   layout.Controls.Add(search, 1, 0);
   layout.Controls.Add(versions, 0, 1);
   layout.Controls.Add(body, 1, 1);
   Controls.Add(layout);
   versions.SelectedIndexChanged += delegate {
    var entry = versions.SelectedItem as Entry;
    body.Text = entry == null ? "No matching entries." : (entry.Title + "\n\n" + entry.Body).Replace("\r\n", "\n").Replace("\n", "\r\n");
    body.SelectionStart = 0;
    body.ScrollToCaret();
   };
   search.TextChanged += delegate { Filter(search.Text.Trim()); };
   Filter("");
  }

  void AddDocument(string document, string fallbackTitle, bool installed) {
   if (String.IsNullOrWhiteSpace(document)) return;
   var headings = Headings.Matches(document);
   if (headings.Count == 0) {
    entries.Insert(0, new Entry { Title = fallbackTitle, Body = document.Trim() });
    return;
   }
   var additions = new List<Entry>();
   for (int i = 0; i < headings.Count; ++i) {
    var heading = headings[i];
    int start = heading.Index + heading.Length;
    int end = i + 1 < headings.Count ? headings[i + 1].Index : document.Length;
    string title = DisplayVersions(heading.Groups[1].Value.Trim());
    string notes = document.Substring(start, end - start).Trim();
    var existing = entries.FirstOrDefault(e => e.Title == title);
    if (existing != null) {
     if (installed && notes.Length > existing.Body.Length) existing.Body = notes;
    } else additions.Add(new Entry { Title = title, Body = notes });
   }
   if (installed) entries.InsertRange(0, additions);
   else {
    entries.AddRange(additions);
    entries.Add(new Entry {
     Title = "0.4.0 | Original port and history notes",
     Body = "FrankHUN88453 released the original CoD3Recomp PC port.\n\n" +
      document.Substring(0, headings[0].Index).Trim()
    });
   }
  }

  void Filter(string query) {
   query = DisplayVersions(query);
   versions.BeginUpdate();
   versions.Items.Clear();
   foreach (var entry in entries)
    if (query.Length == 0 || entry.Title.IndexOf(query, StringComparison.OrdinalIgnoreCase) >= 0 ||
        entry.Body.IndexOf(query, StringComparison.OrdinalIgnoreCase) >= 0) versions.Items.Add(entry);
   versions.EndUpdate();
   if (versions.Items.Count > 0) versions.SelectedIndex = 0;
   else body.Text = "No matching entries.";
  }
 }
}
