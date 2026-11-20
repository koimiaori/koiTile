using System;
using System.Collections.ObjectModel;
using koiTile.Interop;
using koiTile.Models;
using koiTile.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Storage.Pickers;

namespace koiTile.Pages
{
    public sealed partial class InfoPage : Page, ITilePage
    {
        private bool _nameTouched;
        private bool _fillingName;
        private bool _triggerSync;

        public InfoPage()
        {
            InitializeComponent();
            _triggerSync = true;
            TimeTriggerComboBox.Items.Add("Background");
            TimeTriggerComboBox.Items.Add("TrayIcon");
            SystemTriggerComboBox.ItemsSource = Enum.GetValues(typeof(Windows.ApplicationModel.Background.SystemTriggerType));
            TimeTriggerTimeSpan.Value = 30;
            _triggerSync = false;
            Loaded += (s, e) => LoadTile();
            Unloaded += (s, e) => Commit();
        }

        public void LoadTile()
        {
            TileConfig tile = Workspace.Instance.Current;
            bool has = tile != null;
            ShortcutBox.IsEnabled = has;
            TypeBox.IsEnabled = has;
            TriggerTypeComboBox.IsEnabled = has;
            TimeTriggerComboBox.IsEnabled = has;
            TimeTriggerTimeSpan.IsEnabled = has;
            SystemTriggerComboBox.IsEnabled = has;
            if (!has)
            {
                ShortcutBox.Text = "";
                Hint.Text = "没有磁贴";
                return;
            }

            ShortcutBox.Text = tile.Shortcut;
            _fillingName = true;
            _fillingName = false;
            _nameTouched = !string.IsNullOrWhiteSpace(tile.AppName);
            TypeBox.SelectedIndex = tile.LaunchType == 3 ? 1 : 0;
            UpdateBrowseState();
            SyncTrigger(tile);
            SyncFromTile(tile);

            foreach (string item in tile.Queue)
            {
            }
            Hint.Text = "";
        }

        private sealed class TextLine
        {
            public string Style;
            public string Text;
        }

        private sealed class TextItem
        {
            public string Style { get; set; }
            public string Text { get; set; }
        }

        private static readonly System.Text.Json.JsonSerializerOptions LineJson =
            new System.Text.Json.JsonSerializerOptions { PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase };

        private readonly System.Collections.Generic.List<TextLine> _lines = new System.Collections.Generic.List<TextLine>();

        private readonly System.Collections.Generic.Dictionary<TextLine, StackPanel> _rows =
            new System.Collections.Generic.Dictionary<TextLine, StackPanel>();

        private bool _syncing;

        private string _lastXml = "";

        private const string StyleSubtitle = "subtitle";
        private const string StyleBase = "base";
        private const string StyleSubtle = "captionSubtle";
        private const string StyleDefault = "default";

        private static string StyleLabel(string style)
        {
            if (style == StyleSubtitle)
            {
                return "\u6807\u9898";
            }
            if (style == StyleBase)
            {
                return "\u526F\u6807\u9898";
            }
            if (style == StyleSubtle)
            {
                return "\u4E0D\u660E\u663E";
            }
            return "\u6B63\u6587";
        }

        private string Key()
        {
            return Workspace.Instance.Current == null ? "" : Workspace.Instance.Current.Id;
        }

        private StackPanel AddRow(TextLine line)
        {
            var row = new StackPanel { Spacing = 4 };

            var header = new Grid();
            header.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            header.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var label = new TextBlock { Text = StyleLabel(line.Style), VerticalAlignment = VerticalAlignment.Center };
            Grid.SetColumn(label, 0);
            header.Children.Add(label);

            var arrows = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 4, HorizontalAlignment = HorizontalAlignment.Right };
            var up = new Button { Content = "\u25B2", Tag = line, Padding = new Thickness(8, 2, 8, 2), MinWidth = 0, FontSize = 10 };
            up.Click += OnMoveUp;
            arrows.Children.Add(up);
            var down = new Button { Content = "\u25BC", Tag = line, Padding = new Thickness(8, 2, 8, 2), MinWidth = 0, FontSize = 10 };
            down.Click += OnMoveDown;
            arrows.Children.Add(down);
            Grid.SetColumn(arrows, 1);
            header.Children.Add(arrows);
            row.Children.Add(header);

            var body = new Grid { ColumnSpacing = 6 };
            body.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            body.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

            var box = new TextBox { Text = line.Text ?? "", Tag = line };
            box.TextChanged += OnLineChanged;
            Grid.SetColumn(box, 0);
            body.Children.Add(box);

            var remove = new Button { Content = new FontIcon { Glyph = "\uE74D", FontSize = 14 }, Tag = line, VerticalAlignment = VerticalAlignment.Bottom };
            remove.Click += OnRemoveText;
            Grid.SetColumn(remove, 1);
            body.Children.Add(remove);

            row.Children.Add(body);
            _rows[line] = row;
            return row;
        }

        private void RebuildRows()
        {
            if (ContentPanel == null)
            {
                return;
            }
            ContentPanel.Children.Clear();
            _rows.Clear();
            foreach (TextLine line in _lines)
            {
                ContentPanel.Children.Add(AddRow(line));
            }
        }

        private void OnAddText(object sender, RoutedEventArgs e)
        {
            string style = (sender as FrameworkElement)?.Tag as string;
            if (string.IsNullOrEmpty(style) || ContentPanel == null)
            {
                return;
            }
            var line = new TextLine { Style = style, Text = "" };
            _lines.Add(line);
            ContentPanel.Children.Add(AddRow(line));
            SaveToTile(Workspace.Instance.Current);
        }

        private void OnRemoveText(object sender, RoutedEventArgs e)
        {
            var line = (sender as FrameworkElement)?.Tag as TextLine;
            if (line == null)
            {
                return;
            }
            _lines.Remove(line);
            if (_rows.TryGetValue(line, out StackPanel row))
            {
                ContentPanel.Children.Remove(row);
                _rows.Remove(line);
            }
            SaveToTile(Workspace.Instance.Current);
        }

        private void OnMoveUp(object sender, RoutedEventArgs e)
        {
            Move((sender as FrameworkElement)?.Tag as TextLine, -1);
        }

        private void OnMoveDown(object sender, RoutedEventArgs e)
        {
            Move((sender as FrameworkElement)?.Tag as TextLine, 1);
        }

        private void Move(TextLine line, int delta)
        {
            if (line == null || ContentPanel == null)
            {
                return;
            }
            int current = _lines.IndexOf(line);
            int target = current + delta;
            if (current < 0 || target < 0 || target >= _lines.Count)
            {
                return;
            }
            _lines.RemoveAt(current);
            _lines.Insert(target, line);
            RebuildRows();
            SaveToTile(Workspace.Instance.Current);
        }

        private void OnLineChanged(object sender, TextChangedEventArgs e)
        {
            if (_syncing)
            {
                return;
            }
            var box = sender as TextBox;
            var line = box?.Tag as TextLine;
            if (line != null)
            {
                line.Text = box.Text;
            }
            SaveToTile(Workspace.Instance.Current);
        }

        private void SyncXml()
        {
            if (_syncing || REBCustom == null)
            {
                return;
            }
            _syncing = true;
            var builder = new System.Text.StringBuilder();
            foreach (TextLine line in _lines)
            {
                builder.Append("<text hint-style=\"").Append(line.Style).Append("\">").Append(line.Text).Append("</text>");
                builder.Append("\r");
            }
            _lastXml = builder.ToString();
            REBCustom.Document.SetText(Microsoft.UI.Text.TextSetOptions.None, _lastXml);
            _syncing = false;
        }

        private void OnXmlChanged(object sender, RoutedEventArgs e)
        {
            if (_syncing || REBCustom == null || ContentPanel == null)
            {
                return;
            }
            if (PivotRoot == null || PivotRoot.SelectedIndex != 1)
            {
                return;
            }
            string xml = "";
            REBCustom.Document.GetText(Microsoft.UI.Text.TextGetOptions.None, out xml);
            if (xml == _lastXml || string.IsNullOrEmpty(xml))
            {
                return;
            }
            var parsed = new System.Collections.Generic.List<TextLine>();
            foreach (System.Text.RegularExpressions.Match match in System.Text.RegularExpressions.Regex.Matches(xml, "<text[^>]*hint-style=\"([^\"]+)\"[^>]*>(.*?)</text>", System.Text.RegularExpressions.RegexOptions.Singleline))
            {
                parsed.Add(new TextLine { Style = match.Groups[1].Value, Text = match.Groups[2].Value });
            }
            _lines.Clear();
            _lines.AddRange(parsed);
            RebuildRows();
            _lastXml = xml;
            SaveToTile(Workspace.Instance.Current);
        }

        private void SyncFromTile(TileConfig tile)
        {
            if (ContentPanel == null || tile == null)
            {
                return;
            }
            _lines.Clear();
            foreach (string raw in tile.Texts)
            {
                TextLine line = UnpackLine(raw);
                if (line != null)
                {
                    _lines.Add(line);
                }
            }
            if (_lines.Count == 0)
            {
                if (!string.IsNullOrEmpty(tile.Title))
                {
                    _lines.Add(new TextLine { Style = StyleSubtitle, Text = tile.Title });
                }
                if (!string.IsNullOrEmpty(tile.Subtitle))
                {
                    _lines.Add(new TextLine { Style = StyleBase, Text = tile.Subtitle });
                }
            }
            RebuildRows();
            SyncXml();
            if (QueueList != null)
            {
                RefreshQueue(tile);
            }
        }

        private void SaveToTile(TileConfig tile)
        {
            if (tile == null)
            {
                return;
            }
            string title = "";
            string subtitle = "";
            foreach (TextLine line in _lines)
            {
                if (line.Style == StyleSubtitle && title.Length == 0)
                {
                    title = line.Text;
                }
                else if (line.Style == StyleBase && subtitle.Length == 0)
                {
                    subtitle = line.Text;
                }
            }
            tile.Title = title;
            tile.Subtitle = subtitle;
            tile.Texts.Clear();
            foreach (TextLine line in _lines)
            {
                tile.Texts.Add(PackLine(line));
            }
            Workspace.Instance.MarkDirty();
        }

        private static string PackLine(TextLine line)
        {
            var item = new TextItem { Style = line.Style ?? StyleDefault, Text = line.Text ?? "" };
            return System.Text.Json.JsonSerializer.Serialize(item, LineJson);
        }

        private static TextLine UnpackLine(string raw)
        {
            if (string.IsNullOrWhiteSpace(raw))
            {
                return null;
            }
            try
            {
                TextItem item = System.Text.Json.JsonSerializer.Deserialize<TextItem>(raw, LineJson);
                if (item != null && !string.IsNullOrEmpty(item.Style))
                {
                    return new TextLine { Style = item.Style, Text = item.Text ?? "" };
                }
            }
            catch (System.Text.Json.JsonException)
            {
            }
            return null;
        }

        private void OnAddQueue(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            var lines = new System.Collections.Generic.List<QueueLine>();
            foreach (TextLine line in _lines)
            {
                if (string.IsNullOrEmpty(line.Text))
                {
                    continue;
                }
                lines.Add(new QueueLine { Style = line.Style, Text = line.Text });
            }
            if (lines.Count == 0)
            {
                return;
            }
            tile.Queue.Add(QueueLines.Pack(lines));
            RefreshQueue(tile);
            Workspace.Instance.Touch();
        }

        private void RefreshQueue(TileConfig tile)
        {
            if (QueueList == null || tile == null)
            {
                return;
            }
            var items = new System.Collections.Generic.List<QueueViewItem>();
            foreach (string raw in tile.Queue)
            {
                items.Add(QueueLines.View(raw));
            }
            QueueList.ItemsSource = items;
        }

        private void OnPivotChanged(object sender, SelectionChangedEventArgs e)
        {
            if (PivotRoot != null && PivotRoot.SelectedIndex == 1)
            {
                SyncXml();
            }
        }

        private void OnRemoveQueue(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null || QueueList == null)
            {
                return;
            }
            int index = QueueList.SelectedIndex;
            if (index < 0 || index >= tile.Queue.Count)
            {
                return;
            }
            tile.Queue.RemoveAt(index);
            RefreshQueue(tile);
            Workspace.Instance.Touch();
        }

        private void OnApply(object sender, RoutedEventArgs e)
        {
            if (Workspace.Instance.Current == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }
            Commit();
        }

        private void Commit()
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }

            tile.Shortcut = ShortcutBox.Text;
            SaveToTile(tile);
            tile.LaunchType = TypeBox.SelectedIndex == 1 ? 3 : 1;
            if (tile.RefreshSeconds < 30)
            {
                tile.RefreshSeconds = 30;
            }


            Workspace.Instance.Touch();
            Scheduler.Instance.Sync();
        }

        private void OnTypeChanged(object sender, SelectionChangedEventArgs e)
        {
            UpdateBrowseState();
            if (ShortcutBox == null || string.IsNullOrWhiteSpace(ShortcutBox.Text))
            {
                return;
            }
            ApplyName(ShortcutBox.Text, TypeBox.SelectedIndex == 1 ? 3 : 1);
        }

        public void Flush()
        {
            if (Workspace.Instance.Current != null)
            {
                Commit();
            }
        }

        private void OnShortcutLostFocus(object sender, RoutedEventArgs e)
        {
            ApplyName(ShortcutBox.Text, TypeBox.SelectedIndex == 1 ? 3 : 1);
            if (Workspace.Instance.Current != null)
            {
                Commit();
            }
        }

        private async void ApplyName(string value, int type)
        {
            if (_nameTouched || string.IsNullOrWhiteSpace(value))
            {
                return;
            }
            string name = type == 1 ? await System.Threading.Tasks.Task.Run(() => WebsiteTitle(value)) : DeriveName(value, type);
            if (name.Length == 0)
            {
                return;
            }
            _fillingName = true;
            TileConfig current = Workspace.Instance.Current;
            if (current != null)
            {
                current.AppName = name;
            }
            _fillingName = false;
            TileConfig tile = Workspace.Instance.Current;
            if (tile != null)
            {
                tile.AppName = name;
            }
        }

        private string DeriveName(string value, int type)
        {
            string trimmed = value.Trim();
            if (trimmed.Length == 0)
            {
                return "";
            }
            if (type == 3)
            {
                return trimmed;
            }
            if (type == 1)
            {
                return WebsiteTitle(trimmed);
            }
            if (type == 2)
            {
                string leaf = System.IO.Path.GetFileName(trimmed.TrimEnd('\\', '/'));
                return leaf.Length > 0 ? leaf : trimmed;
            }
            string bare = System.IO.Path.GetFileNameWithoutExtension(trimmed);
            return bare.Length > 0 ? bare : trimmed;
        }

        private string WebsiteTitle(string url)
        {
            Uri uri;
            if (!Uri.TryCreate(url, UriKind.Absolute, out uri))
            {
                return url;
            }
            byte[] buffer = new byte[131072];
            string host = uri.Host;
            try
            {
                if (Core.kt_http_get(Core.Utf8(url), buffer, buffer.Length, 8000) == 0)
                {
                    string page = Core.Decode(buffer);
                    var match = System.Text.RegularExpressions.Regex.Match(page, "<title[^>]*>(.*?)</title>", System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.Singleline);
                    if (match.Success)
                    {
                        string title = System.Net.WebUtility.HtmlDecode(match.Groups[1].Value).Trim();
                        if (title.Length > 0)
                        {
                            return title;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("title: " + ex.Message);
            }
            return host.Length > 0 ? host : url;
        }

        private void UpdateBrowseState()
        {
            if (BrowseButton == null || TypeBox == null)
            {
                return;
            }
            int type = TypeBox.SelectedIndex;
            BrowseButton.IsEnabled = true;
        }

        private void OnPickShortcut(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }

            ShowFileMenu();
        }

        private void ShowFileMenu()
        {
            var menu = new MenuFlyout();
            var fileItem = new MenuFlyoutItem { Text = "选择文件" };
            var folderItem = new MenuFlyoutItem { Text = "选择文件夹" };
            fileItem.Click += async (s, e) => await PickFileTarget();
            folderItem.Click += async (s, e) => await PickFolderTarget();
            menu.Items.Add(fileItem);
            menu.Items.Add(folderItem);
            menu.ShowAt(BrowseButton);
        }

        private async System.Threading.Tasks.Task PickFileTarget()
        {
            var picker = new FileOpenPicker();
            WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);
            picker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
            picker.FileTypeFilter.Add("*");
            var file = await picker.PickSingleFileAsync();
            if (file == null)
            {
                return;
            }
            ApplyTarget(file.Path);
        }

        private async System.Threading.Tasks.Task PickFolderTarget()
        {
            var picker = new FolderPicker();
            WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);
            picker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
            picker.FileTypeFilter.Add("*");
            var folder = await picker.PickSingleFolderAsync();
            if (folder == null)
            {
                return;
            }
            ApplyTarget(folder.Path);
        }

        private void ApplyTarget(string path)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.Shortcut = path;
            ShortcutBox.Text = path;
            if (Workspace.Instance.BackgroundFromImport)
            {
                Workspace.Instance.SetBackgroundImage("");
            }
            ApplyName(path, TypeBox.SelectedIndex == 1 ? 3 : 1);
            Workspace.Instance.Touch();
            Hint.Text = "";
        }

        private static int ToInt(NumberBox box, int fallback)
        {
            return double.IsNaN(box.Value) ? fallback : (int)Math.Round(box.Value);
        }

        private void SyncTrigger(TileConfig tile)
        {
            if (TriggerTypeComboBox == null || tile == null)
            {
                return;
            }
            _triggerSync = true;
            int index = tile.TriggerType;
            if (index < 0 || index > 3)
            {
                index = 0;
            }
            TriggerTypeComboBox.SelectedIndex = index;
            TimeTriggerComboBox.SelectedIndex = tile.TriggerTimeKind == 1 ? 1 : 0;
            TimeTriggerTimeSpan.Value = tile.TriggerMinutes < 1 ? 30 : tile.TriggerMinutes;
            SystemTriggerComboBox.SelectedIndex = tile.TriggerSystemType < 0 ? 6 : tile.TriggerSystemType;
            UpdateTriggerVisibility();
            _triggerSync = false;
        }

        private void UpdateTriggerVisibility()
        {
            int index = TriggerTypeComboBox.SelectedIndex;
            bool time = index == 2;
            bool system = index == 3;
            TimeTriggerPanel.Visibility = time ? Visibility.Visible : Visibility.Collapsed;
            SystemTriggerComboBox.Visibility = system ? Visibility.Visible : Visibility.Collapsed;
            TriggerTypeComboBox.CornerRadius = time || system ? new CornerRadius(4, 0, 0, 4) : new CornerRadius(4);
        }

        private void SaveTrigger()
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.TriggerType = TriggerTypeComboBox.SelectedIndex < 0 ? 0 : TriggerTypeComboBox.SelectedIndex;
            tile.TriggerTimeKind = TimeTriggerComboBox.SelectedIndex == 1 ? 1 : 0;
            tile.TriggerMinutes = ToInt(TimeTriggerTimeSpan, 30);
            tile.TriggerSystemType = SystemTriggerComboBox.SelectedIndex < 0 ? 6 : SystemTriggerComboBox.SelectedIndex;
            if (tile.TriggerType == 2 && tile.TriggerTimeKind == 0)
            {
                int minutes = tile.TriggerMinutes < 15 ? 15 : tile.TriggerMinutes;
                tile.RefreshSeconds = minutes * 60;
            }
            Workspace.Instance.Touch();
            Scheduler.Instance.Sync();
        }

        private void OnTriggerTypeChanged(object sender, SelectionChangedEventArgs e)
        {
            if (TriggerTypeComboBox == null || TimeTriggerPanel == null)
            {
                return;
            }
            UpdateTriggerVisibility();
            if (_triggerSync)
            {
                return;
            }
            SaveTrigger();
        }

        private void OnSystemTriggerChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_triggerSync)
            {
                return;
            }
            SaveTrigger();
        }

        private void TimeTriggerComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (TimeTriggerComboBox.SelectedIndex == 0 && TimeTriggerTimeSpan.Value < 15)
            {
                TimeTriggerTimeSpan.Value = 15;
            }
            if (_triggerSync)
            {
                return;
            }
            SaveTrigger();
        }

        private void TimeTriggerTimeSpan_ValueChanged(NumberBox sender, NumberBoxValueChangedEventArgs args)
        {
            if (TimeTriggerComboBox.SelectedIndex == 0 && args.NewValue < 15)
            {
                sender.Value = 15;
            }
            else if (args.NewValue < 1)
            {
                sender.Value = 1;
            }
            if (_triggerSync)
            {
                return;
            }
            SaveTrigger();
        }
    }
}
