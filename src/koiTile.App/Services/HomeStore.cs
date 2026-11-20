using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using koiTile.Models;

namespace koiTile.Services
{
    internal sealed class HomeEntry
    {
        public int Kind { get; set; }
        public int Install { get; set; }
        public DateTime Time { get; set; }
        public string Preview { get; set; } = "";
        public string Label { get; set; } = "";
        public List<string> TileIds { get; set; } = new List<string>();
        public List<TileConfig> Tiles { get; set; } = new List<TileConfig>();
        public string BoardImage { get; set; } = "";
        public int BoardZoom { get; set; } = 100;
        public int BoardPanX { get; set; }
        public int BoardPanY { get; set; }

        [JsonIgnore]
        public Microsoft.UI.Xaml.Media.ImageSource Image { get; set; }

        [JsonIgnore]
        public Microsoft.UI.Xaml.Visibility ImageVisibility
        {
            get
            {
                return Image == null ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
            }
        }

        public void LoadImage()
        {
            Image = null;
            if (string.IsNullOrWhiteSpace(Preview) || !File.Exists(Preview))
            {
                return;
            }
            try
            {
                Image = new Microsoft.UI.Xaml.Media.Imaging.BitmapImage
                {
                    CreateOptions = Microsoft.UI.Xaml.Media.Imaging.BitmapCreateOptions.IgnoreImageCache,
                    UriSource = new Uri(Preview)
                };
            }
            catch (Exception ex)
            {
                Log.Write("home image: " + ex.Message);
            }
        }
    }

    internal static class HomeStore
    {
        private static readonly List<HomeEntry> _entries = new List<HomeEntry>();

        public static IReadOnlyList<HomeEntry> Entries
        {
            get { return _entries; }
        }

        private static string FilePath
        {
            get
            {
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "koiTile", "home.json");
            }
        }

        public static string PreviewDir
        {
            get
            {
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "koiTile", "TileAssets", "home");
            }
        }

        public static void Load()
        {
            _entries.Clear();
            string path = FilePath;
            if (File.Exists(path))
            {
                try
                {
                    List<HomeEntry> list = JsonSerializer.Deserialize<List<HomeEntry>>(File.ReadAllText(path));
                    if (list != null)
                    {
                        foreach (HomeEntry entry in list)
                        {
                            entry.LoadImage();
                            _entries.Add(entry);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Log.Write("home load: " + ex.Message);
                }
            }
            Sort();
            PruneUninstalled();
        }

        public static void Save()
        {
            try
            {
                string path = FilePath;
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                File.WriteAllText(path, JsonSerializer.Serialize(_entries), new System.Text.UTF8Encoding(false));
            }
            catch (Exception ex)
            {
                Log.Write("home save: " + ex.Message);
            }
        }

        public static HomeEntry Add(int kind, string preview, int install)
        {
            var entry = new HomeEntry();
            entry.Kind = kind;
            entry.Install = install;
            entry.Time = DateTime.Now;
            entry.Preview = preview;
            entry.BoardImage = Workspace.Instance.BackgroundImage;
            entry.BoardZoom = Workspace.Instance.BackgroundZoom;
            entry.BoardPanX = Workspace.Instance.BackgroundPanX;
            entry.BoardPanY = Workspace.Instance.BackgroundPanY;
            foreach (TileConfig tile in Workspace.Instance.Tiles)
            {
                entry.Tiles.Add(tile.Clone());
                entry.TileIds.Add(tile.Id);
            }
            if (entry.Tiles.Count > 0)
            {
                entry.Label = entry.Tiles[0].Label;
            }
            entry.LoadImage();
            _entries.Add(entry);
            Sort();
            Save();
            return entry;
        }

        public static void Update(HomeEntry entry, string preview, int install)
        {
            if (entry == null)
            {
                return;
            }
            entry.Install = install;
            if (!string.IsNullOrWhiteSpace(preview))
            {
                entry.Preview = preview;
                entry.LoadImage();
            }
            Capture(entry);
        }

        public static void Capture(HomeEntry entry)
        {
            if (entry == null)
            {
                return;
            }
            entry.BoardImage = Workspace.Instance.BackgroundImage;
            entry.BoardZoom = Workspace.Instance.BackgroundZoom;
            entry.BoardPanX = Workspace.Instance.BackgroundPanX;
            entry.BoardPanY = Workspace.Instance.BackgroundPanY;
            entry.Tiles.Clear();
            entry.TileIds.Clear();
            foreach (TileConfig tile in Workspace.Instance.Tiles)
            {
                entry.Tiles.Add(tile.Clone());
                entry.TileIds.Add(tile.Id);
            }
            if (entry.Tiles.Count > 0)
            {
                entry.Label = entry.Tiles[0].Label;
            }
            Save();
        }

        public static void Remove(HomeEntry entry)
        {
            if (entry == null)
            {
                return;
            }
            RemovePreview(entry.Preview);
            _entries.Remove(entry);
            Save();
        }

        public static void PruneUninstalled()
        {
            bool changed = false;
            for (int i = _entries.Count - 1; i >= 0; i--)
            {
                HomeEntry entry = _entries[i];
                if (entry.Install != 1)
                {
                    continue;
                }
                bool installed = false;
                foreach (string id in entry.TileIds)
                {
                    if (TilePackage.IsInstalled(id))
                    {
                        installed = true;
                        break;
                    }
                }
                if (installed)
                {
                    continue;
                }
                RemovePreview(entry.Preview);
                _entries.RemoveAt(i);
                changed = true;
            }
            if (changed)
            {
                Save();
            }
        }

        private static void RemovePreview(string path)
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                return;
            }
            try
            {
                File.Delete(path);
            }
            catch (Exception ex)
            {
                Log.Write("home preview delete: " + ex.Message);
            }
        }

        private static void Sort()
        {
            _entries.Sort((a, b) => b.Time.CompareTo(a.Time));
        }
    }
}
