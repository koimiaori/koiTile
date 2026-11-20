using System;
using System.Collections.ObjectModel;
using koiTile.Interop;

namespace koiTile.Models
{
    internal sealed class TileConfig : ITileBox
    {
        public const string KeyId = "id";
        public const string KeyName = "name";
        public const string KeyShortcut = "shortcut";
        public const string KeyLaunchType = "launchType";
        public const string KeySize = "size";
        public const string KeyColorEnabled = "colorEnabled";
        public const string KeyColor = "color";
        public const string KeyImage = "image";
        public const string KeyImageOffsetX = "imageOffsetX";
        public const string KeyImageOffsetY = "imageOffsetY";
        public const string KeyImageScale = "imageScale";
        public const string KeyTitle = "title";
        public const string KeySubtitle = "subtitle";
        public const string KeyAppName = "appName";
        public const string KeyLink = "link";
        public const string KeyTextSource = "textSource";
        public const string KeyTextSourcePath = "textSourcePath";
        public const string KeyRefreshSeconds = "refreshSeconds";
        public const string KeyTextTemplate = "textTemplate";
        public const string KeyIconEnabled = "iconEnabled";
        public const string KeyIconPath = "iconPath";
        public const string KeyIconSize = "iconSize";
        public const string KeyIconOffsetX = "iconOffsetX";
        public const string KeyIconOffsetY = "iconOffsetY";
        public const string KeyPeekSeconds = "peekSeconds";
        public const string KeyShowName = "showName";
        public const string KeyNumberAll = "numberAll";
        public const string KeyAutoCrop = "autoCrop";
        public const string KeyPreviewSize = "previewSize";
        public const string KeyUpdateMode = "updateMode";
        public const string KeyPlanMinutes = "planMinutes";
        public const string KeyTriggerType = "triggerType";
        public const string KeyTriggerTimeKind = "triggerTimeKind";
        public const string KeyTriggerMinutes = "triggerMinutes";
        public const string KeyTriggerSystemType = "triggerSystemType";
        public const string KeyImages = "images";
        public const string KeyForegroundLight = "foregroundLight";
        public const string KeyPeek = "peek";
        public const string KeyQueue = "queue";
        public const string KeyTexts = "texts";
        public const string KeyGridCol = "gridCol";
        public const string KeyGridRow = "gridRow";

        public string Id { get; set; } = "";
        public string Name { get; set; } = "";
        public string Shortcut { get; set; } = "";
        public int LaunchType { get; set; }
        public int Size { get; set; } = 3;
        public bool ColorEnabled { get; set; }
        public string Color { get; set; } = "#2D89EF";
        public string Image { get; set; } = "";
        public int ImageOffsetX { get; set; }
        public int ImageOffsetY { get; set; }
        public int ImageScale { get; set; } = 100;
        public string Title { get; set; } = "";
        public string Subtitle { get; set; } = "";
        public string AppName { get; set; } = "";
        public string Link { get; set; } = "";
        public int TextSource { get; set; }
        public string TextSourcePath { get; set; } = "";
        public int RefreshSeconds { get; set; } = 30;
        public string TextTemplate { get; set; } = "{content}";
        public bool IconEnabled { get; set; }
        public string IconPath { get; set; } = "";
        public int IconSize { get; set; } = 64;
        public int IconOffsetX { get; set; }
        public int IconOffsetY { get; set; }
        public int PeekSeconds { get; set; } = 12;
        public bool ShowName { get; set; }
        public bool NumberAll { get; set; }
        public bool AutoCrop { get; set; } = true;
        public int PreviewSize { get; set; } = 3;
        public int[] CropX { get; } = new int[4];
        public int[] CropY { get; } = new int[4];
        public int[] CropZoom { get; } = new int[4];
        public int UpdateMode { get; set; }
        public int PlanMinutes { get; set; } = 30;
        public int TriggerType { get; set; }
        public int TriggerTimeKind { get; set; }
        public int TriggerMinutes { get; set; } = 30;
        public int TriggerSystemType { get; set; } = 6;
        public ObservableCollection<string> Images
        {
            get { return _images; }
            set { _images = value ?? new ObservableCollection<string>(); }
        }

        public bool ForegroundLight { get; set; } = true;
        public int GridCol { get; set; }
        public int GridRow { get; set; }

        public ObservableCollection<string> Peek
        {
            get { return _peek; }
            set { _peek = value ?? new ObservableCollection<string>(); }
        }

        public ObservableCollection<string> Queue
        {
            get { return _queue; }
            set { _queue = value ?? new ObservableCollection<string>(); }
        }

        public ObservableCollection<string> Texts
        {
            get { return _texts; }
            set { _texts = value ?? new ObservableCollection<string>(); }
        }

        private ObservableCollection<string> _images = new ObservableCollection<string>();
        private ObservableCollection<string> _peek = new ObservableCollection<string>();
        private ObservableCollection<string> _queue = new ObservableCollection<string>();
        private ObservableCollection<string> _texts = new ObservableCollection<string>();

        public TileConfig Clone()
        {
            var copy = new TileConfig();
            copy.Id = Id;
            copy.Name = Name;
            copy.Shortcut = Shortcut;
            copy.LaunchType = LaunchType;
            copy.Size = Size;
            copy.ColorEnabled = ColorEnabled;
            copy.Color = Color;
            copy.Image = Image;
            copy.ImageOffsetX = ImageOffsetX;
            copy.ImageOffsetY = ImageOffsetY;
            copy.ImageScale = ImageScale;
            copy.Title = Title;
            copy.Subtitle = Subtitle;
            copy.AppName = AppName;
            copy.Link = Link;
            copy.TextSource = TextSource;
            copy.TextSourcePath = TextSourcePath;
            copy.RefreshSeconds = RefreshSeconds;
            copy.TextTemplate = TextTemplate;
            copy.IconEnabled = IconEnabled;
            copy.IconPath = IconPath;
            copy.IconSize = IconSize;
            copy.IconOffsetX = IconOffsetX;
            copy.IconOffsetY = IconOffsetY;
            copy.PeekSeconds = PeekSeconds;
            copy.ShowName = ShowName;
            copy.NumberAll = NumberAll;
            copy.AutoCrop = AutoCrop;
            copy.PreviewSize = PreviewSize;
            copy.UpdateMode = UpdateMode;
            copy.PlanMinutes = PlanMinutes;
            copy.TriggerType = TriggerType;
            copy.TriggerTimeKind = TriggerTimeKind;
            copy.TriggerMinutes = TriggerMinutes;
            copy.TriggerSystemType = TriggerSystemType;
            foreach (string image in Images)
            {
                copy.Images.Add(image);
            }
            for (int index = 0; index < 4; index++)
            {
                copy.CropX[index] = CropX[index];
                copy.CropY[index] = CropY[index];
                copy.CropZoom[index] = CropZoom[index];
            }
            copy.ForegroundLight = ForegroundLight;
            copy.GridCol = GridCol;
            copy.GridRow = GridRow;
            foreach (string item in Peek)
            {
                copy.Peek.Add(item);
            }
            foreach (string item in Queue)
            {
                copy.Queue.Add(item);
            }
            foreach (string item in Texts)
            {
                copy.Texts.Add(item);
            }
            return copy;
        }
        public string Display
        {
            get { return Label; }
        }

        public string Target
        {
            get { return Label; }
        }

        public string Label
        {
            get
            {
                if (!string.IsNullOrWhiteSpace(AppName))
                {
                    return AppName;
                }
                if (!string.IsNullOrWhiteSpace(Name))
                {
                    return Name;
                }
                return "新建磁贴";
            }
        }

        public static TileConfig Read(IntPtr cfg, string id)
        {
            var tile = new TileConfig();
            tile.Id = id;
            tile.Name = Core.GetBufferString(cfg, id, KeyName);
            tile.Shortcut = Core.GetBufferString(cfg, id, KeyShortcut);
            tile.LaunchType = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyLaunchType), 0);
            tile.Size = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeySize), 3);
            tile.ColorEnabled = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyColorEnabled), 0) != 0;
            tile.Color = Core.GetBufferString(cfg, id, KeyColor);
            tile.Image = Core.GetBufferString(cfg, id, KeyImage);
            tile.ImageOffsetX = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyImageOffsetX), 0);
            tile.ImageOffsetY = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyImageOffsetY), 0);
            tile.ImageScale = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyImageScale), 100);
            tile.Title = Core.GetBufferString(cfg, id, KeyTitle);
            tile.Subtitle = Core.GetBufferString(cfg, id, KeySubtitle);
            tile.AppName = Core.GetBufferString(cfg, id, KeyAppName);
            tile.Link = Core.GetBufferString(cfg, id, KeyLink);
            tile.TextSource = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyTextSource), 0);
            tile.TextSourcePath = Core.GetBufferString(cfg, id, KeyTextSourcePath);
            tile.RefreshSeconds = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyRefreshSeconds), 30);
            tile.TextTemplate = Core.GetBufferString(cfg, id, KeyTextTemplate);
            tile.IconEnabled = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyIconEnabled), 0) != 0;
            tile.IconPath = Core.GetBufferString(cfg, id, KeyIconPath);
            tile.IconSize = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyIconSize), 64);
            tile.IconOffsetX = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyIconOffsetX), 0);
            tile.IconOffsetY = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyIconOffsetY), 0);
            tile.PeekSeconds = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyPeekSeconds), 12);
            tile.ShowName = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyShowName), 0) != 0;
            tile.NumberAll = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyNumberAll), 0) != 0;
            tile.AutoCrop = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyAutoCrop), 1) != 0;
            tile.PreviewSize = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyPreviewSize), tile.Size);
            tile.UpdateMode = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyUpdateMode), 0);
            tile.PlanMinutes = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyPlanMinutes), 30);
            tile.TriggerType = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyTriggerType), 0);
            tile.TriggerTimeKind = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyTriggerTimeKind), 0);
            tile.TriggerMinutes = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyTriggerMinutes), 30);
            tile.TriggerSystemType = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyTriggerSystemType), 6);
            int imageCount = Core.kt_config_list_count(cfg, Core.Utf8(id), Core.Utf8(KeyImages));
            for (int imageIndex = 0; imageIndex < imageCount; imageIndex++)
            {
                tile.Images.Add(Core.GetListString(cfg, id, KeyImages, imageIndex));
            }
            for (int index = 0; index < 4; index++)
            {
                tile.CropX[index] = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8("cropX" + index), 0);
                tile.CropY[index] = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8("cropY" + index), 0);
                tile.CropZoom[index] = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8("cropZoom" + index), 0);
            }
            tile.ForegroundLight = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyForegroundLight), 1) != 0;
            tile.GridCol = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyGridCol), 0);
            tile.GridRow = Core.kt_config_get_int(cfg, Core.Utf8(id), Core.Utf8(KeyGridRow), 0);

            int peekCount = Core.kt_config_list_count(cfg, Core.Utf8(id), Core.Utf8(KeyPeek));
            for (int i = 0; i < peekCount; i++)
            {
                tile.Peek.Add(Core.GetListString(cfg, id, KeyPeek, i));
            }

            int queueCount = Core.kt_config_list_count(cfg, Core.Utf8(id), Core.Utf8(KeyQueue));
            for (int i = 0; i < queueCount; i++)
            {
                tile.Queue.Add(Core.GetListString(cfg, id, KeyQueue, i));
            }

            int textCount = Core.kt_config_list_count(cfg, Core.Utf8(id), Core.Utf8(KeyTexts));
            for (int i = 0; i < textCount; i++)
            {
                tile.Texts.Add(Core.GetListString(cfg, id, KeyTexts, i));
            }

            return tile;
        }

        public void Write(IntPtr cfg)
        {
            byte[] id = Core.Utf8(Id);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyName), Core.Utf8(Name));
            Core.kt_config_set(cfg, id, Core.Utf8(KeyShortcut), Core.Utf8(Shortcut));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyLaunchType), LaunchType);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeySize), Size);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyColorEnabled), ColorEnabled ? 1 : 0);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyColor), Core.Utf8(Color));
            Core.kt_config_set(cfg, id, Core.Utf8(KeyImage), Core.Utf8(Image));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyImageOffsetX), ImageOffsetX);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyImageOffsetY), ImageOffsetY);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyImageScale), ImageScale);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyTitle), Core.Utf8(Title));
            Core.kt_config_set(cfg, id, Core.Utf8(KeySubtitle), Core.Utf8(Subtitle));
            Core.kt_config_set(cfg, id, Core.Utf8(KeyAppName), Core.Utf8(AppName));
            Core.kt_config_set(cfg, id, Core.Utf8(KeyLink), Core.Utf8(Link));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyTextSource), TextSource);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyTextSourcePath), Core.Utf8(TextSourcePath));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyRefreshSeconds), RefreshSeconds);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyTextTemplate), Core.Utf8(TextTemplate));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyIconEnabled), IconEnabled ? 1 : 0);
            Core.kt_config_set(cfg, id, Core.Utf8(KeyIconPath), Core.Utf8(IconPath));
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyIconSize), IconSize);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyIconOffsetX), IconOffsetX);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyIconOffsetY), IconOffsetY);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyPeekSeconds), PeekSeconds);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyShowName), ShowName ? 1 : 0);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyNumberAll), NumberAll ? 1 : 0);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyAutoCrop), AutoCrop ? 1 : 0);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyPreviewSize), PreviewSize);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyUpdateMode), UpdateMode);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyPlanMinutes), PlanMinutes);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyTriggerType), TriggerType);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyTriggerTimeKind), TriggerTimeKind);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyTriggerMinutes), TriggerMinutes);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyTriggerSystemType), TriggerSystemType);
            Core.kt_config_list_clear(cfg, id, Core.Utf8(KeyImages));
            foreach (string image in Images)
            {
                Core.kt_config_list_add(cfg, id, Core.Utf8(KeyImages), Core.Utf8(image));
            }
            for (int index = 0; index < 4; index++)
            {
                Core.kt_config_set_int(cfg, id, Core.Utf8("cropX" + index), CropX[index]);
                Core.kt_config_set_int(cfg, id, Core.Utf8("cropY" + index), CropY[index]);
                Core.kt_config_set_int(cfg, id, Core.Utf8("cropZoom" + index), CropZoom[index]);
            }
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyForegroundLight), ForegroundLight ? 1 : 0);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyGridCol), GridCol);
            Core.kt_config_set_int(cfg, id, Core.Utf8(KeyGridRow), GridRow);

            Core.kt_config_list_clear(cfg, id, Core.Utf8(KeyPeek));
            foreach (string item in Peek)
            {
                Core.kt_config_list_add(cfg, id, Core.Utf8(KeyPeek), Core.Utf8(item));
            }

            Core.kt_config_list_clear(cfg, id, Core.Utf8(KeyQueue));
            foreach (string item in Queue)
            {
                Core.kt_config_list_add(cfg, id, Core.Utf8(KeyQueue), Core.Utf8(item));
            }

            Core.kt_config_list_clear(cfg, id, Core.Utf8(KeyTexts));
            foreach (string item in Texts)
            {
                Core.kt_config_list_add(cfg, id, Core.Utf8(KeyTexts), Core.Utf8(item));
            }
        }
    }
}
