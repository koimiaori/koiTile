using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using koiTile.Interop;
using koiTile.Models;

namespace koiTile.Services
{
    internal sealed class Workspace : IDisposable
    {
        public static readonly Workspace Instance = new Workspace();

        private sealed class Document
        {
            public IntPtr Handle;
            public string Path = "";
            public ObservableCollection<TileConfig> Tiles { get; } = new ObservableCollection<TileConfig>();
            public TileConfig Current;
            public bool Dirty;
            public bool Loaded;
            public string Image = "";
            public bool FromImport;
            public int Zoom = 100;
            public int PanX;
            public int PanY;
        }

        private readonly Document[] _docs = { new Document(), new Document() };
        private int _mode = 1;
        private IntPtr _scratch;

        private Document Active
        {
            get { return _docs[_mode]; }
        }

        public int Mode
        {
            get { return _mode; }
        }

        public IntPtr Config
        {
            get { return Active.Handle; }
        }

        public string ConfigPath
        {
            get { return Active.Path; }
        }

        public string AppDir { get; private set; } = "";

        public string AssetDir { get; private set; } = "";

        public string TempDir { get; private set; } = "";

        public string PreviewDir { get; private set; } = "";

        public ObservableCollection<TileConfig> Tiles
        {
            get { return Active.Tiles; }
        }

        public TileConfig Current
        {
            get { return Active.Current; }
        }

        public bool Dirty
        {
            get { return Active.Dirty; }
        }

        public int PeekIndex { get; private set; }

        public bool FreePosition { get; set; }

        public bool Guide { get; set; }

        public static string SizeLabel(int size)
        {
            if (size == 0)
            {
                return "小号";
            }
            if (size == 1)
            {
                return "中号";
            }
            if (size == 2)
            {
                return "宽号";
            }
            return "大号";
        }

        public int GuideIndex(TileConfig tile)
        {
            var ordered = new List<TileConfig>(Tiles);
            ordered.Sort((a, b) => a.GridRow != b.GridRow ? a.GridRow.CompareTo(b.GridRow) : a.GridCol.CompareTo(b.GridCol));
            return ordered.IndexOf(tile) + 1;
        }

        public void RefreshView()
        {
            BackgroundChanged?.Invoke();
        }

        public string BackgroundImage
        {
            get { return Active.Image; }
        }

        public bool BackgroundFromImport
        {
            get { return Active.FromImport; }
        }

        public int BackgroundZoom
        {
            get { return Active.Zoom; }
        }

        public int BackgroundPanX
        {
            get { return Active.PanX; }
        }

        public int BackgroundPanY
        {
            get { return Active.PanY; }
        }

        public int ScaleWidth
        {
            get { return _mode == 0 ? TileGrid.SpanPixels(4) : TileGrid.Width(Tiles); }
        }

        public int ScaleHeight
        {
            get { return _mode == 0 ? TileGrid.SpanPixels(4) : TileGrid.Height(Tiles); }
        }

        public event Action PreviewChanged;

        public event Action ListChanged;

        public event Action CurrentChanged;

        public event Action BackgroundChanged;
        public event Action<TileConfig> LocalTileOpened;

        private Workspace()
        {
        }

        public void Start()
        {
            string groupPath = Core.Get(Core.kt_config_path, 1024);
            AppDir = Core.Get(Core.kt_appdata_dir, 1024);
            AssetDir = Core.Get(Core.kt_tileassets_dir, 1024);
            TempDir = Core.Get(Core.kt_temp_dir, 1024);
            PreviewDir = Path.Combine(TempDir, "preview");
            Directory.CreateDirectory(PreviewDir);
            _docs[1].Path = groupPath;
            _docs[0].Path = Path.Combine(AppDir, "tile-config.json");
            _scratch = Core.kt_config_new();
            LoadDocument(_docs[1], false);
        }

        public bool SetMode(int mode)
        {
            if (mode < 0 || mode > 1 || mode == _mode)
            {
                return false;
            }
            _mode = mode;
            if (!Active.Loaded)
            {
                LoadDocument(Active, _mode == 0);
            }
            if (_mode == 0)
            {
                NormalizeSingle();
            }
            ListChanged?.Invoke();
            CurrentChanged?.Invoke();
            BackgroundChanged?.Invoke();
            PreviewChanged?.Invoke();
            return true;
        }

        public void Reload()
        {
            LoadDocument(Active, _mode == 0);
            ListChanged?.Invoke();
            CurrentChanged?.Invoke();
            PreviewChanged?.Invoke();
        }

        private void LoadDocument(Document doc, bool single)
        {
            doc.Tiles.Clear();
            if (doc.Handle != IntPtr.Zero)
            {
                Core.kt_config_free(doc.Handle);
                doc.Handle = IntPtr.Zero;
            }
            doc.Handle = Core.kt_config_load(Core.Utf8(doc.Path));
            if (doc.Handle == IntPtr.Zero)
            {
                doc.Handle = Core.kt_config_new();
            }
            int count = Core.kt_config_tile_count(doc.Handle);
            for (int i = 0; i < count; i++)
            {
                byte[] buffer = new byte[512];
                if (Core.kt_config_tile_id_at(doc.Handle, i, buffer, buffer.Length) != 0)
                {
                    continue;
                }
                doc.Tiles.Add(TileConfig.Read(doc.Handle, Core.Decode(buffer)));
            }
            if (doc.Tiles.Count == 0)
            {
                if (single)
                {
                    doc.Tiles.Add(NewSingleTile(doc.Tiles));
                }
                else
                {
                    TileGrid.Initialize(doc.Tiles, () => NextIdFor(doc.Tiles));
                }
            }
            TileGrid.Clamp(doc.Tiles);
            if (single)
            {
                NormalizeSingle(doc);
            }
            doc.Current = doc.Tiles.Count > 0 ? doc.Tiles[0] : null;
            ReadBackground(doc);
            doc.Dirty = false;
            doc.Loaded = true;
        }

        private void ReadBackground(Document doc)
        {
            doc.Image = Core.BoardString(doc.Handle, "image");
            doc.Zoom = Core.BoardInt(doc.Handle, "zoom", 100);
            doc.PanX = Core.BoardInt(doc.Handle, "panX", 0);
            doc.PanY = Core.BoardInt(doc.Handle, "panY", 0);
            doc.FromImport = false;
            if (!string.IsNullOrWhiteSpace(doc.Image))
            {
                return;
            }
            foreach (TileConfig tile in doc.Tiles)
            {
                if (!string.IsNullOrWhiteSpace(tile.Image))
                {
                    doc.Image = tile.Image;
                    doc.Zoom = 100;
                    doc.PanX = 0;
                    doc.PanY = 0;
                    return;
                }
            }
        }

        private static TileConfig NewSingleTile(IList<TileConfig> tiles)
        {
            return new TileConfig
            {
                Id = NextIdFor(tiles),
                Size = 3,
                PreviewSize = 3,
                ShowName = false,
                GridCol = 0,
                GridRow = 0
            };
        }

        private void NormalizeSingle()
        {
            NormalizeSingle(Active);
        }

        private static void NormalizeSingle(Document doc)
        {
            if (doc.Tiles.Count == 0)
            {
                doc.Tiles.Add(NewSingleTile(doc.Tiles));
                return;
            }
            while (doc.Tiles.Count > 1)
            {
                doc.Tiles.RemoveAt(doc.Tiles.Count - 1);
            }
            TileConfig tile = doc.Tiles[0];
            if (tile.PreviewSize < 0 || tile.PreviewSize > 3)
            {
                tile.PreviewSize = 3;
            }
            tile.Size = tile.PreviewSize == 2 ? 2 : 3;
            tile.GridCol = 0;
            tile.GridRow = 0;
            if (!ReferenceEquals(doc.Current, tile))
            {
                doc.Current = tile;
            }
        }

        public string NextId()
        {
            return NextIdFor(Tiles);
        }

        private static string NextIdFor(IList<TileConfig> tiles)
        {
            int index = 1;
            while (true)
            {
                string id = "tile-" + index.ToString();
                bool used = false;
                foreach (TileConfig tile in tiles)
                {
                    if (tile.Id == id)
                    {
                        used = true;
                        break;
                    }
                }
                if (!used)
                {
                    return id;
                }
                index++;
            }
        }

        public void NotifyListChanged()
        {
            if (Active.Current == null || !Tiles.Contains(Active.Current))
            {
                Active.Current = Tiles.Count > 0 ? Tiles[0] : null;
            }
            ListChanged?.Invoke();
            CurrentChanged?.Invoke();
        }

        public void RemoveTiles(IEnumerable<TileConfig> tiles)
        {
            FlushAll();
            foreach (TileConfig tile in tiles)
            {
                if (tile == null)
                {
                    continue;
                }
                if (Core.kt_tile_restore(Config, Core.Utf8(tile.Id)) != 0)
                {
                    Log.Write("restore failed " + tile.Id + ": " + Core.LastError());
                }
            }
        }

        public bool SelectById(string id)
        {
            foreach (TileConfig tile in Tiles)
            {
                if (tile.Id == id)
                {
                    Select(tile);
                    return true;
                }
            }
            return false;
        }

        public bool RemoveInstalled(string id)
        {
            if (string.IsNullOrWhiteSpace(id))
            {
                return false;
            }
            FlushAll();
            if (Core.kt_tile_restore(Config, Core.Utf8(id)) != 0)
            {
                Log.Write("remove installed failed " + id + ": " + Core.LastError());
                return false;
            }
            return true;
        }

        public void OpenTile(string id)
        {
            foreach (TileConfig tile in Tiles)
            {
                if (tile.Id == id)
                {
                    Select(tile);
                    LocalTileOpened?.Invoke(tile);
                    return;
                }
            }
        }

        public void Select(TileConfig tile)
        {
            if (tile == null || ReferenceEquals(tile, Active.Current))
            {
                return;
            }
            Active.Current = tile;
            CurrentChanged?.Invoke();
        }

        public void Touch()
        {
            Active.Dirty = true;
            PreviewChanged?.Invoke();
        }

        public void MarkDirty()
        {
            Active.Dirty = true;
        }

        public void LoadBackground()
        {
            ReadBackground(Active);
        }

        public string TileLabel(TileConfig tile)
        {
            if (tile == null)
            {
                return "";
            }
            string name = tile.Label;
            if (!tile.NumberAll || Tiles.Count < 2)
            {
                return name;
            }
            int index = 0;
            int found = -1;
            foreach (TileConfig item in OrderedTiles())
            {
                if (ReferenceEquals(item, tile))
                {
                    found = index;
                    break;
                }
                index++;
            }
            if (found < 0)
            {
                return name;
            }
            return name + " " + (found + 1).ToString("00");
        }

        private List<TileConfig> OrderedTiles()
        {
            var list = new List<TileConfig>(Tiles);
            list.Sort((a, b) =>
            {
                if (a.GridRow != b.GridRow)
                {
                    return a.GridRow.CompareTo(b.GridRow);
                }
                return a.GridCol.CompareTo(b.GridCol);
            });
            return list;
        }

        public void StoreBackgroundView(int zoom, int panX, int panY)
        {
            if (Active.Zoom == zoom && Active.PanX == panX && Active.PanY == panY)
            {
                return;
            }
            Active.Zoom = zoom;
            Active.PanX = panX;
            Active.PanY = panY;
            Core.SetBoardString(Active.Handle, "image", Active.Image);
            Core.SetBoardInt(Active.Handle, "zoom", zoom);
            Core.SetBoardInt(Active.Handle, "panX", panX);
            Core.SetBoardInt(Active.Handle, "panY", panY);
            Active.Dirty = true;
        }

        public void SetImportedBackground(string path)
        {
            SetBackgroundImage(path);
            Active.FromImport = !string.IsNullOrWhiteSpace(path);
        }

        public void SetBackgroundImage(string path)
        {
            Active.Image = path ?? "";
            Active.FromImport = false;
            Active.Zoom = 100;
            Active.PanX = 0;
            Active.PanY = 0;
            Core.SetBoardString(Active.Handle, "image", Active.Image);
            Core.SetBoardInt(Active.Handle, "zoom", Active.Zoom);
            Core.SetBoardInt(Active.Handle, "panX", Active.PanX);
            Core.SetBoardInt(Active.Handle, "panY", Active.PanY);
            Active.Dirty = true;
            TileConfig current = Active.Current;
            if (current != null)
            {
                for (int index = 0; index < 4; index++)
                {
                    current.CropX[index] = 0;
                    current.CropY[index] = 0;
                }
            }
            BackgroundChanged?.Invoke();
        }

        public ObservableCollection<string> Images
        {
            get
            {
                TileConfig tile = Current;
                return tile == null ? null : tile.Images;
            }
        }

        public void AddImage(string path)
        {
            TileConfig tile = Current;
            if (tile == null || string.IsNullOrWhiteSpace(path))
            {
                return;
            }
            bool first = tile.Images.Count == 0;
            if (!tile.Images.Contains(path))
            {
                tile.Images.Add(path);
            }
            if (first)
            {
                SetBackgroundImage(path);
                return;
            }
            SelectImage(path);
        }

        public void SelectImage(string path)
        {
            if (string.IsNullOrWhiteSpace(path) || Active.Image == path)
            {
                return;
            }
            Active.Image = path;
            Active.FromImport = false;
            Core.SetBoardString(Active.Handle, "image", Active.Image);
            Active.Dirty = true;
            BackgroundChanged?.Invoke();
        }

        public int CropSlot(int size)
        {
            TileConfig tile = Current;
            if (tile == null || tile.AutoCrop || size < 0 || size > 3)
            {
                return 3;
            }
            return size;
        }

        public void CropView(int size, out int zoom, out int panX, out int panY)
        {
            TileConfig tile = Current;
            if (tile == null)
            {
                zoom = 100;
                panX = 0;
                panY = 0;
                return;
            }
            if (tile.AutoCrop || size == tile.PreviewSize)
            {
                zoom = Active.Zoom <= 0 ? 100 : Active.Zoom;
                panX = Active.PanX;
                panY = Active.PanY;
                return;
            }
            int slot = CropSlot(size);
            zoom = tile.CropZoom[slot] <= 0 ? 100 : tile.CropZoom[slot];
            panX = tile.CropX[slot];
            panY = tile.CropY[slot];
        }

        public void ApplyPreviewSize(int size)
        {
            if (size < 0 || size > 3 || Current == null)
            {
                return;
            }
            Current.PreviewSize = size;
            Current.Size = size == 2 ? 2 : 3;
        }

        public void SaveCrop()
        {
            if (_mode != 0 || Current == null)
            {
                return;
            }
            int slot = CropSlot(Current.PreviewSize);
            Current.CropZoom[slot] = Active.Zoom;
            Current.CropX[slot] = Active.PanX;
            Current.CropY[slot] = Active.PanY;
        }

        public void SaveCropAll()
        {
            if (_mode != 0 || Current == null)
            {
                return;
            }
            for (int index = 0; index < 4; index++)
            {
                Current.CropZoom[index] = Active.Zoom;
                Current.CropX[index] = Active.PanX;
                Current.CropY[index] = Active.PanY;
            }
        }

        public void ApplyCrop()
        {
            if (_mode != 0 || Current == null)
            {
                return;
            }
            int slot = CropSlot(Current.PreviewSize);
            Active.Zoom = Current.CropZoom[slot] <= 0 ? 100 : Current.CropZoom[slot];
            Active.PanX = Current.CropX[slot];
            Active.PanY = Current.CropY[slot];
        }

        public void RestoreSnapshot(List<TileConfig> tiles, string image, int zoom, int panX, int panY)
        {
            Tiles.Clear();
            if (tiles != null)
            {
                foreach (TileConfig tile in tiles)
                {
                    Tiles.Add(tile.Clone());
                }
            }
            if (_mode == 0)
            {
                NormalizeSingle();
            }
            Core.SetBoardString(Active.Handle, "image", image ?? "");
            Core.SetBoardInt(Active.Handle, "zoom", zoom);
            Core.SetBoardInt(Active.Handle, "panX", panX);
            Core.SetBoardInt(Active.Handle, "panY", panY);
            Active.Image = image ?? "";
            Active.FromImport = false;
            Active.Zoom = zoom;
            Active.PanX = panX;
            Active.PanY = panY;
            FlushAll();
            Active.Current = Tiles.Count == 0 ? null : Tiles[0];
            Active.Dirty = true;
            ListChanged?.Invoke();
            BackgroundChanged?.Invoke();
            CurrentChanged?.Invoke();
            PreviewChanged?.Invoke();
        }

        private void Flush(Document doc)
        {
            Core.kt_config_clear(doc.Handle);
            foreach (TileConfig tile in doc.Tiles)
            {
                Core.kt_config_add_tile(doc.Handle, Core.Utf8(tile.Id));
                tile.Write(doc.Handle);
            }
        }

        public void FlushAll()
        {
            Flush(Active);
        }

        private bool Write(Document doc)
        {
            if (!doc.Loaded)
            {
                return true;
            }
            if (Core.kt_config_save(doc.Handle, Core.Utf8(doc.Path)) != 0)
            {
                Log.Write("save failed " + doc.Path + ": " + Core.LastError());
                return false;
            }
            doc.Dirty = false;
            return true;
        }

        public bool Save()
        {
            Flush(Active);
            return Write(Active);
        }

        public bool SaveAll()
        {
            bool ok = true;
            foreach (Document doc in _docs)
            {
                if (!doc.Loaded)
                {
                    continue;
                }
                Flush(doc);
                ok &= Write(doc);
            }
            return ok;
        }

        public bool Apply(TileConfig tile)
        {
            FlushAll();
            if (Core.kt_tile_apply(Config, Core.Utf8(tile.Id)) != 0)
            {
                Log.Write("apply failed " + tile.Id + ": " + Core.LastError());
                return false;
            }
            Core.kt_tile_config_save_tile(Config, Core.Utf8(tile.Id));
            return true;
        }

        public void ResetDocument()
        {
            Tiles.Clear();
            Active.Current = null;
            Active.Image = "";
            Active.FromImport = false;
            Active.Zoom = 100;
            Active.PanX = 0;
            Active.PanY = 0;
            Core.SetBoardString(Active.Handle, "image", "");
            Core.SetBoardInt(Active.Handle, "zoom", 100);
            Core.SetBoardInt(Active.Handle, "panX", 0);
            Core.SetBoardInt(Active.Handle, "panY", 0);
            if (_mode == 0)
            {
                Tiles.Add(NewSingleTile(Tiles));
            }
            else
            {
                TileGrid.Initialize(Tiles, NextId);
            }
            Active.Current = Tiles.Count > 0 ? Tiles[0] : null;
            FlushAll();
            Active.Dirty = false;
            ListChanged?.Invoke();
            BackgroundChanged?.Invoke();
            CurrentChanged?.Invoke();
            PreviewChanged?.Invoke();
        }

        public void ResetCurrentTile()
        {
            TileConfig tile = Current;
            if (tile == null)
            {
                return;
            }
            tile.ColorEnabled = false;
            tile.Color = "";
            tile.Images.Clear();
            tile.Texts.Clear();
            tile.Queue.Clear();
            tile.Peek.Clear();
            tile.Title = "";
            tile.Subtitle = "";
            tile.IconEnabled = false;
            tile.IconPath = "";
            tile.IconSize = 64;
            tile.IconOffsetX = 0;
            tile.IconOffsetY = 0;
            tile.TextSource = 0;
            tile.TextSourcePath = "";
            tile.TextTemplate = "{content}";
            tile.TriggerType = 0;
            tile.TriggerTimeKind = 0;
            tile.TriggerMinutes = 30;
            tile.TriggerSystemType = 6;
            tile.UpdateMode = 0;
            tile.PlanMinutes = 30;
            tile.RefreshSeconds = 30;
            for (int index = 0; index < 4; index++)
            {
                tile.CropX[index] = 0;
                tile.CropY[index] = 0;
                tile.CropZoom[index] = 0;
            }
            SetBackgroundImage("");
            MarkDirty();
        }

        public bool RestoreAll()
        {
            FlushAll();
            bool ok = Core.kt_tile_restore_all(Config) == 0;
            if (File.Exists(ConfigPath))
            {
                try
                {
                    File.Delete(ConfigPath);
                }
                catch (Exception ex)
                {
                    Log.Write(ex.Message);
                }
            }
            try
            {
                if (Directory.Exists(PreviewDir))
                {
                    Directory.Delete(PreviewDir, true);
                    Directory.CreateDirectory(PreviewDir);
                }
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
            }
            Tiles.Clear();
            Active.Current = null;
            Active.Image = "";
            Active.Dirty = false;
            ListChanged?.Invoke();
            CurrentChanged?.Invoke();
            PreviewChanged?.Invoke();
            return ok;
        }

        public bool InstallStart(TileConfig tile)
        {
            if (tile == null)
            {
                return false;
            }
            int size = tile.Size;
            if (_mode == 0)
            {
                tile.Size = tile.PreviewSize;
                FlushAll();
            }
            bool ok = Core.kt_tile_install_start(Config, Core.Utf8(tile.Id)) == 0;
            if (ok)
            {
                if (_mode == 0)
                {
                    tile.Size = size;
                    FlushAll();
                }
                return true;
            }
            if (_mode == 0)
            {
                tile.Size = size;
                FlushAll();
            }
            Log.Write("install failed " + tile.Id + ": " + Core.LastError());
            return false;
        }

        public string PeekImage(TileConfig tile)
        {
            if (tile == null || tile.Peek.Count == 0)
            {
                return string.Empty;
            }
            int index = PeekIndex % tile.Peek.Count;
            if (index < 0)
            {
                index = 0;
            }
            return tile.Peek[index];
        }

        public void AdvancePeek(TileConfig tile)
        {
            if (tile == null || tile.Peek.Count == 0)
            {
                PeekIndex = 0;
                return;
            }
            PeekIndex = (PeekIndex + 1) % tile.Peek.Count;
        }

        public void Dispose()
        {
            foreach (Document doc in _docs)
            {
                if (doc.Handle != IntPtr.Zero)
                {
                    Core.kt_config_free(doc.Handle);
                    doc.Handle = IntPtr.Zero;
                }
            }
            if (_scratch != IntPtr.Zero)
            {
                Core.kt_config_free(_scratch);
                _scratch = IntPtr.Zero;
            }
        }
    }
}
