using System;
using System.Collections.Generic;
using koiTile.Interop;
using koiTile.Models;
using koiTile.Services;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Automation.Peers;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.UI.Xaml.Shapes;
using Windows.Foundation;
using Windows.System;
using Windows.UI;

namespace koiTile.Controls
{
    public sealed partial class TileBoard : UserControl
    {
        private const double MinZoom = 100;
        private const double MaxZoom = 800;
        private const double HandleSize = 26;
        private const double FrameThickness = 4;
        private const double FrameGap = 4;
        private const double FrameInset = FrameThickness + FrameGap;
        private const double HandleInset = 6;
        private const double HandleOffset = FrameInset - HandleInset + HandleSize / 2;
        private const double DragThreshold = 3;
        private const double RingOpacity = 0.55;

        private readonly Dictionary<TileConfig, TileHost> _hosts = new Dictionary<TileConfig, TileHost>();
        private readonly Dictionary<TileConfig, Border> _rings = new Dictionary<TileConfig, Border>();
        private readonly Dictionary<TileConfig, Grid> _fills = new Dictionary<TileConfig, Grid>();
        private readonly Dictionary<TileConfig, Border> _underFills = new Dictionary<TileConfig, Border>();
        private readonly Dictionary<TileConfig, TextBlock> _labels = new Dictionary<TileConfig, TextBlock>();
        private readonly Dictionary<TileConfig, Border> _guides = new Dictionary<TileConfig, Border>();
        private readonly Dictionary<TileConfig, TextBlock> _guideTexts = new Dictionary<TileConfig, TextBlock>();
        private bool _guide;
        private readonly Dictionary<TileConfig, Image> _icons = new Dictionary<TileConfig, Image>();
        private bool _chrome = true;
        private readonly List<(TileConfig Tile, int Size, int Column, int Row)> _dragOrigin = new List<(TileConfig Tile, int Size, int Column, int Row)>();
        private readonly List<TileConfig> _dragAdded = new List<TileConfig>();

        private IList<TileConfig> _items = new List<TileConfig>();
        private bool _busy;
        private TileConfig _selected;
        private Point _pressPoint;
        private Point _lastPoint;
        private bool _panning;
        private bool _handleDragging;
        private int _handleColumns;
        private int _handleRows;
        private Border _frame;
        private bool _dark;
        private Border _handle;

        private string _bgPath = "";
        private int _bgImageWidth;
        private int _bgImageHeight;
        private double _bgZoom = 100;
        private double _bgOffsetX;
        private double _bgOffsetY;
        private bool _bgReset;
        private double _bgScale;
        private double _bgCenterX;
        private double _bgCenterY;

        public TileBoard()
        {
            InitializeComponent();
            AutomationProperties.SetAutomationId(this, "board");
            Workspace.Instance.BackgroundChanged += OnBackgroundChanged;
        }

        protected override AutomationPeer OnCreateAutomationPeer()
        {
            return new FrameworkElementAutomationPeer(this);
        }

        internal event Action SelectionChanged;

        internal event Action ViewChanged;

        private bool _singleTile;

        internal bool SingleTile
        {
            get { return _singleTile; }
            set
            {
                if (_singleTile == value)
                {
                    return;
                }
                _singleTile = value;
                Refresh();
            }
        }

        internal bool Guide
        {
            get { return _guide; }
            set
            {
                if (_guide == value)
                {
                    return;
                }
                _guide = value;
                Refresh();
            }
        }

        internal bool IsDark
        {
            get { return _dark; }
        }

        private int DisplaySize(TileConfig tile)
        {
            if (_singleTile && tile.PreviewSize >= TileGrid.MinSize && tile.PreviewSize <= TileGrid.MaxSize)
            {
                return tile.PreviewSize;
            }
            return tile.Size;
        }

        internal void ReloadView()
        {
            RestorePlacement();
            _bgReset = false;
            Refresh();
        }

        internal IList<TileConfig> Items
        {
            get { return _items; }
            set
            {
                _items = value ?? new List<TileConfig>();
                Refresh();
            }
        }

        internal TileConfig Selected
        {
            get { return _selected; }
        }

        internal void Select(TileConfig tile)
        {
            if (tile == null || ReferenceEquals(_selected, tile))
            {
                return;
            }
            _selected = tile;
            UpdateSelectionVisuals();
            SelectionChanged?.Invoke();
        }


        internal void Refresh()
        {
            if (_busy)
            {
                return;
            }
            _busy = true;
            try
            {
                if (_selected != null && !_items.Contains(_selected))
                {
                    _selected = null;
                }
                SyncCore();
            }
            finally
            {
                _busy = false;
            }
        }

        private void SyncCore()
        {
            var gone = new List<TileConfig>();
            foreach (KeyValuePair<TileConfig, TileHost> pair in _hosts)
            {
                if (!_items.Contains(pair.Key))
                {
                    gone.Add(pair.Key);
                }
            }
            foreach (TileConfig tile in gone)
            {
                CellLayer.Children.Remove(_hosts[tile]);
                _hosts.Remove(tile);
                _rings.Remove(tile);
                _fills.Remove(tile);
                _labels.Remove(tile);
                if (_underFills.TryGetValue(tile, out Border under))
                {
                    FillLayer.Children.Remove(under);
                    _underFills.Remove(tile);
                }
                _guides.Remove(tile);
                _guideTexts.Remove(tile);
                if (_icons.ContainsKey(tile))
                {
                    IconLayer.Children.Remove(_icons[tile]);
                    _icons.Remove(tile);
                }
            }

            foreach (TileConfig tile in _items)
            {
                Rect box = TileGrid.BoxOf(tile);
                if (!_hosts.TryGetValue(tile, out TileHost host))
                {
                    var ring = new Border { BorderThickness = new Thickness(2) };
                    var label = new TextBlock
                    {
                        HorizontalAlignment = HorizontalAlignment.Left,
                        VerticalAlignment = VerticalAlignment.Bottom,
                        Foreground = new SolidColorBrush(Colors.White),
                        IsHitTestVisible = false,
                        TextWrapping = TextWrapping.Wrap,
                        MaxLines = 2,
                        TextTrimming = TextTrimming.CharacterEllipsis
                    };
                    var guideText = new TextBlock
                    {
                        Foreground = new SolidColorBrush(Colors.Black),
                        FontWeight = Microsoft.UI.Text.FontWeights.Bold,
                        TextAlignment = TextAlignment.Center,
                        IsHitTestVisible = false,
                        LineHeight = 14
                    };
                    var guide = new Border
                    {
                        Background = new SolidColorBrush(Colors.White),
                        Padding = new Thickness(4, 2, 4, 2),
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment = VerticalAlignment.Center,
                        IsHitTestVisible = false,
                        Visibility = Visibility.Collapsed,
                        Child = guideText
                    };
                    var under = new Border { Background = TileFill(tile), IsHitTestVisible = false };
                    FillLayer.Children.Add(under);
                    _underFills[tile] = under;
                    var fill = new Grid { Children = { ring, label, guide } };
                    host = new TileHost
                    {
                        Tag = tile,
                        Content = fill
                    };
                    AutomationProperties.SetAutomationId(host, "tile-" + tile.Id);
                    CellLayer.Children.Add(host);
                    _hosts[tile] = host;
                    _rings[tile] = ring;
                    _fills[tile] = fill;
                    _labels[tile] = label;
                    _guides[tile] = guide;
                    _guideTexts[tile] = guideText;
                    var icon = new Image { Stretch = Stretch.Uniform, IsHitTestVisible = false };
                    IconLayer.Children.Add(icon);
                    _icons[tile] = icon;
                }

                host.Width = box.Width;
                host.Height = box.Height;
                AutomationProperties.SetName(host, tile.Display);
                Canvas.SetLeft(host, TileGrid.Margin + box.X);
                Canvas.SetTop(host, TileGrid.Margin + box.Y);
                Border underFill = _underFills[tile];
                underFill.Width = box.Width;
                underFill.Height = box.Height;
                underFill.Background = TileFill(tile);
                Canvas.SetLeft(underFill, TileGrid.Margin + box.X);
                Canvas.SetTop(underFill, TileGrid.Margin + box.Y);
                TextBlock caption = _labels[tile];
                bool small = DisplaySize(tile) == TileGrid.MinSize;
                caption.Visibility = _singleTile || small || !tile.ShowName ? Visibility.Collapsed : Visibility.Visible;
                caption.Text = tile.ShowName ? Workspace.Instance.TileLabel(tile) : "";
                caption.FontSize = 12;
                double inset = Math.Max(2, Math.Round(box.Height * 0.06));
                caption.Margin = new Thickness(inset + 2, 0, inset, inset - 1);
                caption.MaxWidth = Math.Max(8, box.Width - inset * 2);
                UpdateIcon(tile, box);
            }

            int gridWidth = TileGrid.Width(_items);
            int gridHeight = TileGrid.Height(_items);
            Width = gridWidth + TileGrid.Margin * 2;
            Height = gridHeight + TileGrid.Margin * 2;
            Board.Width = Width;
            Board.Height = Height;
            Board.Clip = new RectangleGeometry { Rect = new Rect(0, 0, Width, Height) };
            ImageLayer.Clip = new RectangleGeometry
            {
                Rect = new Rect(TileGrid.Margin, TileGrid.Margin, gridWidth, gridHeight)
            };
            DrawBackground();
            DrawLines();
            DrawButtons();
            ApplyGuides();
            UpdateSelectionVisuals();
        }

        private void ApplyGuides()
        {
            if (_guide)
            {
                var ordered = new List<TileConfig>(_items);
                ordered.Sort((a, b) => a.GridRow != b.GridRow ? a.GridRow.CompareTo(b.GridRow) : a.GridCol.CompareTo(b.GridCol));
                for (int i = 0; i < ordered.Count; i++)
                {
                    TileConfig item = ordered[i];
                    if (!_guideTexts.TryGetValue(item, out TextBlock text))
                    {
                        continue;
                    }
                    text.Text = Workspace.SizeLabel(item.Size) + "\n" + (i + 1).ToString("00");
                    text.FontSize = item.Size == 3 ? 26 : (item.Size == 2 ? 19 : (item.Size == 1 ? 15 : 10));
                }
            }
            foreach (KeyValuePair<TileConfig, Border> pair in _guides)
            {
                pair.Value.Visibility = _guide && _chrome ? Visibility.Visible : Visibility.Collapsed;
            }
        }

        private void OnBackgroundChanged()
        {
            _bgReset = true;
            Refresh();
            ApplyBackgroundToTiles();
        }

        internal void ApplyTheme(bool dark)
        {
            _dark = dark;
            Refresh();
        }

        private Rect Box()
        {
            return new Rect(TileGrid.Margin, TileGrid.Margin, TileGrid.Width(_items), TileGrid.Height(_items));
        }

        private void EnsureBackground()
        {
            string path = Workspace.Instance.BackgroundImage ?? "";
            if (path == _bgPath)
            {
                return;
            }

            _bgPath = path;
            _bgImageWidth = 0;
            _bgImageHeight = 0;
            _bgZoom = 100;
            _bgOffsetX = 0;
            _bgOffsetY = 0;
            BackgroundImage.Source = null;

            if (_bgPath.Length == 0)
            {
                return;
            }

            int width = 0;
            int height = 0;
            if (Core.kt_image_info(Core.Utf8(_bgPath), ref width, ref height) != 0 || width <= 0 || height <= 0)
            {
                _bgPath = "";
                return;
            }

            _bgImageWidth = width;
            _bgImageHeight = height;
            BackgroundImage.Source = LoadImage(_bgPath);

            if (!_bgReset)
            {
                RestorePlacement();
            }
            _bgReset = false;
        }

        private void RestorePlacement()
        {
            _bgZoom = Workspace.Instance.BackgroundZoom;
            _bgOffsetX = Workspace.Instance.BackgroundPanX;
            _bgOffsetY = Workspace.Instance.BackgroundPanY;
        }

        private void DrawBackground()
        {
            EnsureBackground();
            if (_bgImageWidth <= 0 || _bgImageHeight <= 0)
            {
                BackgroundImage.Source = null;
                ViewChanged?.Invoke();
                return;
            }

            Rect box = Box();
            double refWidth = Workspace.Instance.ScaleWidth;
            double refHeight = Workspace.Instance.ScaleHeight;
            double cover = Math.Max(refWidth / _bgImageWidth, refHeight / _bgImageHeight);
            _bgScale = cover * _bgZoom / 100.0;
            double width = _bgImageWidth * _bgScale;
            double height = _bgImageHeight * _bgScale;

            double limitX = Math.Max(0, (width - refWidth) / 2.0);
            double limitY = Math.Max(0, (height - refHeight) / 2.0);
            if (Workspace.Instance.FreePosition)
            {
                limitX = double.MaxValue;
                limitY = double.MaxValue;
            }
            _bgOffsetX = Math.Max(-limitX, Math.Min(limitX, _bgOffsetX));
            _bgOffsetY = Math.Max(-limitY, Math.Min(limitY, _bgOffsetY));

            _bgCenterX = box.X + box.Width / 2.0 + _bgOffsetX;
            _bgCenterY = box.Y + box.Height / 2.0 + _bgOffsetY;
            Workspace.Instance.StoreBackgroundView((int)Math.Round(_bgZoom), (int)Math.Round(_bgOffsetX), (int)Math.Round(_bgOffsetY));
            BackgroundImage.Width = width;
            BackgroundImage.Height = height;
            Canvas.SetLeft(BackgroundImage, _bgCenterX - width / 2.0);
            Canvas.SetTop(BackgroundImage, _bgCenterY - height / 2.0);
            ApplyBackgroundToTiles();
            ViewChanged?.Invoke();
        }

        private void DrawLines()
        {
            LineLayer.Children.Clear();
            Brush stroke = LineBrush();
            int columns = TileGrid.Columns(_items);
            int rows = TileGrid.Rows(_items);
            var owners = new TileConfig[columns * rows];
            foreach (TileConfig tile in _items)
            {
                int span = TileGrid.SizeCols(tile.Size);
                int depth = TileGrid.SizeRows(tile.Size);
                for (int r = 0; r < depth; r++)
                {
                    for (int c = 0; c < span; c++)
                    {
                        int column = tile.GridCol + c;
                        int row = tile.GridRow + r;
                        if (column < 0 || row < 0 || column >= columns || row >= rows)
                        {
                            continue;
                        }
                        owners[row * columns + column] = tile;
                    }
                }
            }

            double half = TileGrid.Gap / 2.0;
            for (int row = 0; row < rows; row++)
            {
                for (int column = 0; column < columns; column++)
                {
                    TileConfig self = owners[row * columns + column];
                    double startX = TileGrid.Margin + TileGrid.ColumnLeft(column) - half;
                    double startY = TileGrid.Margin + row * TileGrid.Step - half;
                    if (column % TileGrid.GroupSize != TileGrid.GroupSize - 1 && column + 1 < columns && !ReferenceEquals(self, owners[row * columns + column + 1]))
                    {
                        double x = startX + TileGrid.Step;
                        AddLine(stroke, x, startY, x, startY + TileGrid.Step);
                    }
                    if (row + 1 < rows && !ReferenceEquals(self, owners[(row + 1) * columns + column]))
                    {
                        double y = startY + TileGrid.Step;
                        AddLine(stroke, startX, y, startX + TileGrid.Step, y);
                    }
                }
            }

            double gridHeight = TileGrid.Height(_items);
            for (int column = TileGrid.GroupSize - 1; column + 1 < columns; column += TileGrid.GroupSize)
            {
                var band = new Rectangle
                {
                    Width = TileGrid.ColumnLeft(column + 1) - TileGrid.ColumnLeft(column) - TileGrid.Unit,
                    Height = gridHeight,
                    Fill = stroke
                };
                Canvas.SetLeft(band, TileGrid.Margin + TileGrid.ColumnLeft(column) + TileGrid.Unit);
                Canvas.SetTop(band, TileGrid.Margin);
                LineLayer.Children.Add(band);
            }
        }

        private void AddLine(Brush stroke, double x1, double y1, double x2, double y2)
        {
            LineLayer.Children.Add(new Line
            {
                X1 = x1,
                Y1 = y1,
                X2 = x2,
                Y2 = y2,
                Stroke = stroke,
                StrokeThickness = TileGrid.Gap
            });
        }

        private void DrawButtons()
        {
            double gridWidth = TileGrid.Width(_items);
            double gridHeight = TileGrid.Height(_items);
            Brush accent = ThemeBrush("AccentFillColorDefaultBrush", Color.FromArgb(255, 0, 120, 212));

            if (ButtonLayer.Children.Count == 0)
            {
                _frame = new Border
                {
                    BorderThickness = new Thickness(FrameThickness),
                    IsHitTestVisible = false
                };
                ButtonLayer.Children.Add(_frame);

                _handle = new Border
                {
                    Width = HandleSize,
                    Height = HandleSize,
                    CornerRadius = new CornerRadius(4)
                };
                AutomationProperties.SetName(_handle, "handle");
                AutomationProperties.SetAutomationId(_handle, "resizeHandle");
                _handle.PointerPressed += OnHandlePointerPressed;
                _handle.PointerMoved += OnHandlePointerMoved;
                _handle.PointerReleased += OnHandlePointerReleased;
                _handle.PointerCaptureLost += OnHandlePointerReleased;
                ButtonLayer.Children.Add(_handle);
            }

            _frame.Width = gridWidth + FrameInset * 2;
            _frame.Height = gridHeight + FrameInset * 2;
            _frame.BorderBrush = accent;
            Canvas.SetLeft(_frame, TileGrid.Margin - FrameInset);
            Canvas.SetTop(_frame, TileGrid.Margin - FrameInset);

            _handle.Background = accent;
            Canvas.SetLeft(_handle, TileGrid.Margin + gridWidth + FrameInset - HandleInset);
            Canvas.SetTop(_handle, TileGrid.Margin + gridHeight + FrameInset - HandleInset);
            _handle.Visibility = _chrome && !_singleTile ? Visibility.Visible : Visibility.Collapsed;
        }

        private void ApplyBackgroundToTiles()
        {
            bool hasImage = _bgImageWidth > 0 && _bgImageHeight > 0;
            foreach (TileConfig tile in _items)
            {
                if (!hasImage)
                {
                    tile.Image = "";
                    continue;
                }
                Rect cell = TileGrid.RectOf(tile);
                double centerX = TileGrid.Margin + cell.X + cell.Width / 2.0;
                double centerY = TileGrid.Margin + cell.Y + cell.Height / 2.0;
                tile.Image = _bgPath;
                tile.ImageScale = (int)Math.Round(_bgScale * 100.0);
                tile.ImageOffsetX = (int)Math.Round(_bgCenterX - centerX);
                tile.ImageOffsetY = (int)Math.Round(_bgCenterY - centerY);
            }
            Workspace.Instance.MarkDirty();
        }

        private void UpdateIcon(TileConfig tile, Rect box)
        {
            if (!_icons.TryGetValue(tile, out Image icon))
            {
                return;
            }
            string path = string.IsNullOrWhiteSpace(tile.IconPath) ? (tile.Shortcut ?? "") : tile.IconPath;
            bool show = !_singleTile && tile.IconEnabled;
            ImageSource source = show ? TileIcons.Source(path) : null;
            icon.Source = source;
            icon.Visibility = source == null ? Visibility.Collapsed : Visibility.Visible;
            if (source == null)
            {
                return;
            }
            double size = TileIcons.Size(DisplaySize(tile));
            icon.Width = size;
            icon.Height = size;
            double centerX = TileGrid.Margin + box.X + (box.Width / 2.0) + tile.IconOffsetX;
            double centerY = TileGrid.Margin + box.Y + (box.Height / 2.0) + tile.IconOffsetY - 2;
            Canvas.SetLeft(icon, centerX - (size / 2.0));
            Canvas.SetTop(icon, centerY - (size / 2.0));
        }

        internal void SetChrome(bool visible)
        {
            _chrome = visible;
            ButtonLayer.Visibility = visible ? Visibility.Visible : Visibility.Collapsed;
            ApplyGuides();
            UpdateSelectionVisuals();
        }

        private void UpdateSelectionVisuals()
        {
            foreach (KeyValuePair<TileConfig, Grid> pair in _fills)
            {

            }
            foreach (KeyValuePair<TileConfig, Border> pair in _rings)
            {
                pair.Value.BorderBrush = _chrome && !_singleTile && ReferenceEquals(pair.Key, _selected) ? SelectionBrush() : null;
            }
        }

        private Brush TileFill(TileConfig tile)
        {
            if (tile != null && tile.ColorEnabled)
            {
                return new SolidColorBrush(ParseColor(tile.Color));
            }
            return new SolidColorBrush(Colors.Transparent);
        }

        private Brush SelectionBrush()
        {
            return new SolidColorBrush(Colors.White) { Opacity = RingOpacity };
        }

        private Brush LineBrush()
        {
            if (!_dark)
            {
                return new SolidColorBrush(Color.FromArgb(255, 0xE4, 0xE4, 0xE4));
            }
            return ThemeBrush("ApplicationPageBackgroundThemeBrush", Colors.Gray);
        }

        private static Brush ThemeBrush(string key, Color fallback)
        {
            try
            {
                Brush brush = Application.Current.Resources[key] as Brush;
                if (brush != null)
                {
                    return brush;
                }
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
            }
            return new SolidColorBrush(fallback);
        }

        internal static Color ParseColor(string hex)
        {
            if (string.IsNullOrEmpty(hex) || hex.Length != 7 || hex[0] != '#')
            {
                return Colors.White;
            }
            try
            {
                byte r = Convert.ToByte(hex.Substring(1, 2), 16);
                byte g = Convert.ToByte(hex.Substring(3, 2), 16);
                byte b = Convert.ToByte(hex.Substring(5, 2), 16);
                return Color.FromArgb(255, r, g, b);
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
                return Colors.White;
            }
        }

        private static BitmapImage LoadImage(string path)
        {
            try
            {
                return new BitmapImage
                {
                    CreateOptions = BitmapCreateOptions.IgnoreImageCache,
                    UriSource = new Uri(path)
                };
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
                return null;
            }
        }

        private void OnHandlePointerPressed(object sender, PointerRoutedEventArgs e)
        {
            if (_items.Count == 0)
            {
                return;
            }
            _dragOrigin.Clear();
            _dragAdded.Clear();
            foreach (TileConfig tile in _items)
            {
                _dragOrigin.Add((tile, tile.Size, tile.GridCol, tile.GridRow));
            }
            _handleColumns = TileGrid.Columns(_items);
            _handleRows = TileGrid.Rows(_items);
            ((UIElement)sender).CapturePointer(e.Pointer);
            _handleDragging = true;
            e.Handled = true;
        }

        private void OnHandlePointerMoved(object sender, PointerRoutedEventArgs e)
        {
            if (!_handleDragging)
            {
                return;
            }
            Point point = e.GetCurrentPoint(Board).Position;
            int columns = Snap(point.X - TileGrid.Margin - HandleOffset, TileGrid.MaxColumns);
            int rows = Snap(point.Y - TileGrid.Margin - HandleOffset, int.MaxValue);
            if (columns == _handleColumns && rows == _handleRows)
            {
                return;
            }
            _handleColumns = columns;
            _handleRows = rows;
            ApplyExtent(columns, rows);
            e.Handled = true;
        }

        private void OnHandlePointerReleased(object sender, PointerRoutedEventArgs e)
        {
            if (!_handleDragging)
            {
                return;
            }
            _handleDragging = false;
            ((UIElement)sender).ReleasePointerCapture(e.Pointer);
            _dragOrigin.Clear();
            _dragAdded.Clear();
            Workspace.Instance.MarkDirty();
            Workspace.Instance.NotifyListChanged();
            Refresh();
            e.Handled = true;
        }

        private static int Snap(double pixels, int limit)
        {
            return TileGrid.UnitsAt(pixels, limit);
        }

        private void ApplyExtent(int columns, int rows)
        {
            var tiles = Workspace.Instance.Tiles;
            foreach (TileConfig added in _dragAdded)
            {
                tiles.Remove(added);
            }
            _dragAdded.Clear();
            foreach ((TileConfig Tile, int Size, int Column, int Row) entry in _dragOrigin)
            {
                entry.Tile.Size = entry.Size;
                entry.Tile.GridCol = entry.Column;
                entry.Tile.GridRow = entry.Row;
                if (!tiles.Contains(entry.Tile))
                {
                    tiles.Add(entry.Tile);
                }
            }

            for (int i = tiles.Count - 1; i >= 0; i--)
            {
                TileConfig tile = tiles[i];
                while (tile.GridCol + TileGrid.SizeCols(tile.Size) > columns || tile.GridRow + TileGrid.SizeRows(tile.Size) > rows)
                {
                    if (tile.Size <= TileGrid.MinSize)
                    {
                        tiles.RemoveAt(i);
                        break;
                    }
                    tile.Size--;
                }
            }

            TileGrid.Fill(tiles, 0, 0, columns, rows, 0, 0, 0, 0, Workspace.Instance.NextId);

            foreach (TileConfig tile in tiles)
            {
                _dragAdded.Add(tile);
            }
            foreach ((TileConfig Tile, int Size, int Column, int Row) entry in _dragOrigin)
            {
                _dragAdded.Remove(entry.Tile);
            }

            Workspace.Instance.MarkDirty();
            Refresh();
        }

        private static bool IsButton(object source)
        {
            var element = source as DependencyObject;
            while (element != null)
            {
                if (element is Button)
                {
                    return true;
                }
                element = VisualTreeHelper.GetParent(element);
            }
            return false;
        }

        private TileConfig HitTile(Point point, object source)
        {
            TileConfig tile = FindTile(source);
            if (tile != null)
            {
                return tile;
            }
            double x = point.X - TileGrid.Margin;
            double y = point.Y - TileGrid.Margin;
            foreach (TileConfig item in _items)
            {
                Rect rect = TileGrid.RectOf(item);
                if (x >= rect.X && x <= rect.X + rect.Width && y >= rect.Y && y <= rect.Y + rect.Height)
                {
                    return item;
                }
            }
            return null;
        }

        private static TileConfig FindTile(object source)
        {
            var element = source as DependencyObject;
            while (element != null)
            {
                if (element is FrameworkElement framework && framework.Tag is TileConfig tile)
                {
                    return tile;
                }
                element = VisualTreeHelper.GetParent(element);
            }
            return null;
        }

        private void OnBoardPointerPressed(object sender, PointerRoutedEventArgs e)
        {
            if (IsButton(e.OriginalSource))
            {
                return;
            }

            Board.Focus(FocusState.Programmatic);
            Point press = e.GetCurrentPoint(Board).Position;
            TileConfig hit = HitTile(press, e.OriginalSource);
            if (hit != null)
            {
                Select(hit);
            }
            _pressPoint = e.GetCurrentPoint(Board).Position;
            _lastPoint = _pressPoint;
            _panning = true;
            Board.CapturePointer(e.Pointer);
        }

        private void OnBoardPointerMoved(object sender, PointerRoutedEventArgs e)
        {
            if (!_panning)
            {
                return;
            }

            Point point = e.GetCurrentPoint(Board).Position;
            _bgOffsetX += point.X - _lastPoint.X;
            _bgOffsetY += point.Y - _lastPoint.Y;
            _lastPoint = point;
            DrawBackground();
            if (_singleTile)
            {
                Workspace.Instance.SaveCrop();
            }
        }

        private void OnBoardPointerReleased(object sender, PointerRoutedEventArgs e)
        {
            Point point = e.GetCurrentPoint(Board).Position;
            bool dragged = Distance(point, _pressPoint) > DragThreshold;

            _panning = false;
            Board.ReleasePointerCaptures();

            if (dragged)
            {
                ApplyBackgroundToTiles();
                if (_singleTile)
                {
                    Workspace.Instance.SaveCrop();
                }
            }
        }

        private void OnBoardPointerCaptureLost(object sender, PointerRoutedEventArgs e)
        {
            _panning = false;
        }

        private void OnBoardPointerWheelChanged(object sender, PointerRoutedEventArgs e)
        {
            if (_bgImageWidth <= 0 || _bgImageHeight <= 0)
            {
                return;
            }

            int delta = e.GetCurrentPoint(Board).Properties.MouseWheelDelta;
            _bgZoom = Math.Max(MinZoom, Math.Min(MaxZoom, _bgZoom * (1.0 + delta / 1200.0)));
            DrawBackground();
            ApplyBackgroundToTiles();
            if (_singleTile)
            {
                Workspace.Instance.SaveCrop();
            }
            e.Handled = true;
        }

        private static double Distance(Point a, Point b)
        {
            double dx = a.X - b.X;
            double dy = a.Y - b.Y;
            return Math.Sqrt(dx * dx + dy * dy);
        }
    }
}
