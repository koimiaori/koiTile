using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;
using System.Text.Json;
using Windows.Data.Xml.Dom;
using Windows.UI.Notifications;

internal sealed class Slide
{
    public string Small = "";
    public string Medium = "";
    public string Wide = "";
    public string Large = "";
}

internal sealed class TileText
{
    public string Style = "default";
    public string Text = "";
}

internal static class Program
{
    private const int PlanDays = 7;
    private static string _library = "";
    private static string _title = "";
    private static string _subtitle = "";
    private static string _text = "";
    private static readonly List<TileText> _texts = new List<TileText>();

    [STAThread]
    private static int Main(string[] args)
    {
        bool scheduleOnly = false;
        foreach (string arg in args)
        {
            if (string.Equals(arg, "--schedule", StringComparison.OrdinalIgnoreCase))
            {
                scheduleOnly = true;
            }
        }
        try
        {
            _library = ReadLibrary();
            if (!string.IsNullOrEmpty(_library))
            {
                AssemblyLoadContext.Default.Resolving += Resolve;
            }
            if (!scheduleOnly)
            {
                LaunchTarget();
            }
            ApplySchedule();
            System.Threading.Thread.Sleep(700);
            return 0;
        }
        catch (Exception ex)
        {
            Log(ex.ToString());
            return 1;
        }
    }

    private static Assembly Resolve(AssemblyLoadContext context, AssemblyName name)
    {
        try
        {
            string path = Path.Combine(_library, name.Name + ".dll");
            if (File.Exists(path))
            {
                return context.LoadFromAssemblyPath(path);
            }
        }
        catch (Exception ex)
        {
            Log("resolve " + name.Name + ": " + ex.Message);
        }
        return null;
    }

    private static string ReadLibrary()
    {
        try
        {
            string config = Path.Combine(AppContext.BaseDirectory, "tile.json");
            if (!File.Exists(config))
            {
                return "";
            }
            using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(config));
            if (doc.RootElement.TryGetProperty("lib", out JsonElement lib))
            {
                string value = lib.GetString() ?? "";
                return Directory.Exists(value) ? value : "";
            }
        }
        catch (Exception ex)
        {
            Log("lib: " + ex.Message);
        }
        return "";
    }

    private static void LaunchTarget()
    {
        string config = Path.Combine(AppContext.BaseDirectory, "tile.json");
        if (!File.Exists(config))
        {
            return;
        }
        using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(config));
        JsonElement element = doc.RootElement;
        string target = element.TryGetProperty("target", out JsonElement t) ? (t.GetString() ?? "") : "";
        string arguments = element.TryGetProperty("args", out JsonElement a) ? (a.GetString() ?? "") : "";
        string workDir = element.TryGetProperty("workdir", out JsonElement w) ? (w.GetString() ?? "") : "";
        if (string.IsNullOrWhiteSpace(target))
        {
            Log("empty target");
            return;
        }
        var info = new ProcessStartInfo(target)
        {
            UseShellExecute = true,
            Arguments = arguments ?? ""
        };
        if (!string.IsNullOrEmpty(workDir) && Directory.Exists(workDir))
        {
            info.WorkingDirectory = workDir;
        }
        Process.Start(info);
    }

    private static void ApplySchedule()
    {
        string file = Path.Combine(AppContext.BaseDirectory, "schedule.json");
        if (!File.Exists(file))
        {
            return;
        }

        var slides = new List<Slide>();
        int mode;
        int interval;
        int index;
        int minutes;
        using (JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file)))
        {
            JsonElement root = doc.RootElement;
            mode = root.TryGetProperty("mode", out JsonElement m) ? m.GetInt32() : 0;
            interval = root.TryGetProperty("interval", out JsonElement v) ? v.GetInt32() : 30;
            minutes = root.TryGetProperty("minutes", out JsonElement mn) ? mn.GetInt32() : 30;
            index = root.TryGetProperty("index", out JsonElement i) ? i.GetInt32() : 0;
            _title = root.TryGetProperty("title", out JsonElement ti) ? (ti.GetString() ?? "") : "";
            _subtitle = root.TryGetProperty("subtitle", out JsonElement su) ? (su.GetString() ?? "") : "";
            _text = root.TryGetProperty("text", out JsonElement tx) ? (tx.GetString() ?? "") : "";
            if (root.TryGetProperty("slides", out JsonElement array) && array.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement item in array.EnumerateArray())
                {
                    slides.Add(new Slide
                    {
                        Small = Str(item, "small"),
                        Medium = Str(item, "medium"),
                        Wide = Str(item, "wide"),
                        Large = Str(item, "large")
                    });
                }
            }
            _texts.Clear();
            if (root.TryGetProperty("texts", out JsonElement texts) && texts.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement item in texts.EnumerateArray())
                {
                    string value = Str(item, "text");
                    if (string.IsNullOrEmpty(value))
                    {
                        continue;
                    }
                    string style = Str(item, "style");
                    _texts.Add(new TileText
                    {
                        Style = string.IsNullOrEmpty(style) ? "default" : style,
                        Text = value
                    });
                }
            }
            if (_texts.Count == 0)
            {
                if (!string.IsNullOrWhiteSpace(_title))
                {
                    _texts.Add(new TileText { Style = "caption", Text = _title });
                }
                if (!string.IsNullOrWhiteSpace(_subtitle))
                {
                    _texts.Add(new TileText { Style = "captionSubtle", Text = _subtitle });
                }
                if (!string.IsNullOrWhiteSpace(_text))
                {
                    _texts.Add(new TileText { Style = "body", Text = _text });
                }
            }
        }

        if (slides.Count == 0)
        {
            return;
        }
        if (interval <= 0)
        {
            interval = 30;
        }

        string identity;
        try
        {
            identity = Windows.ApplicationModel.Package.Current.Id.Name;
        }
        catch (Exception ex)
        {
            identity = "none: " + ex.GetType().Name;
        }
        Log("schedule identity=" + identity + " mode=" + mode + " slots=" + slides.Count);

        TileUpdater updater = TileUpdateManager.CreateTileUpdaterForApplication();
        List<ScheduledTileNotification> pending = new List<ScheduledTileNotification>(updater.GetScheduledTileNotifications());
        foreach (ScheduledTileNotification item in pending)
        {
            updater.RemoveFromSchedule(item);
        }
        Log("schedule pending=" + pending.Count);
        updater.EnableNotificationQueue(false);
        updater.Clear();
        DateTimeOffset now = DateTimeOffset.Now;

        if (mode == 2)
        {
            int step = minutes <= 0 ? 30 : minutes;
            int total = (PlanDays * 24 * 60) / step;
            if (total > 96)
            {
                total = 96;
            }
            for (int slot = 1; slot <= total; slot++)
            {
                Schedule(updater, slides, now.AddMinutes((double)step * slot), slot);
            }
            return;
        }

        if (mode == 1)
        {
            int pick = index % slides.Count;
            if (pick < 0)
            {
                pick = 0;
            }
            var single = new List<Slide> { slides[pick] };
            var notification = new TileNotification(_texts.Count > 0 ? BuildTextXml(slides[pick]) : BuildXml(single));
            notification.Tag = "koiTile";
            updater.Update(notification);
            Log("click slide=" + pick + " of " + slides.Count + " xml=" + notification.Content.GetXml());
            WriteIndex(file, mode, interval, minutes, (pick + 1) % slides.Count);
            return;
        }

        if (_texts.Count > 0)
        {
            updater.EnableNotificationQueue(true);
            int count = Math.Min(slides.Count, 5);
            for (int slot = 0; slot < count; slot++)
            {
                var notification = new TileNotification(BuildTextXml(slides[slot]));
                notification.Tag = "koiTile" + slot.ToString();
                updater.Update(notification);
            }
            Log("text pushed=" + count + " of " + slides.Count + " xml=" + BuildTextXml(slides[0]).GetXml());
            return;
        }

        var show = new TileNotification(BuildXml(slides));
        show.Tag = "koiTile";
        updater.Update(show);
        Log("photos pushed=" + slides.Count + " xml=" + show.Content.GetXml());
    }

    private static void Schedule(TileUpdater updater, List<Slide> slides, DateTimeOffset delivery, int slot)
    {
        XmlDocument xml = _texts.Count > 0
            ? BuildTextXml(slides[slot % slides.Count])
            : BuildXml(slides);
        var scheduled = new ScheduledTileNotification(xml, delivery);
        scheduled.Tag = "koiTile" + slot.ToString();
        updater.AddToSchedule(scheduled);
    }

    private static void WriteIndex(string file, int mode, int interval, int minutes, int index)
    {
        try
        {
            JsonElement slides;
            using (JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file)))
            {
                if (!doc.RootElement.TryGetProperty("slides", out JsonElement array))
                {
                    return;
                }
                slides = array.Clone();
            }
            var payload = new Dictionary<string, object>
            {
                ["mode"] = mode,
                ["interval"] = interval,
                ["minutes"] = minutes,
                ["index"] = index,
                ["slides"] = slides
            };
            File.WriteAllText(file, JsonSerializer.Serialize(payload), new UTF8Encoding(false));
        }
        catch (Exception ex)
        {
            Log("index: " + ex.Message);
        }
    }

    private static XmlDocument BuildXml(List<Slide> slides)
    {
        var builder = new StringBuilder();
        builder.Append("<tile><visual>");
        builder.Append(Binding("TileLarge", slides, slide => slide.Large, 2));
        builder.Append(Binding("TileMedium", slides, slide => slide.Medium, 2));
        builder.Append(Binding("TileWide", slides, slide => slide.Wide, 2));
        builder.Append(Binding("TileSmall", slides, slide => slide.Small, 1));
        builder.Append("</visual></tile>");
        var document = new XmlDocument();
        document.LoadXml(builder.ToString());
        return document;
    }

    private static XmlDocument BuildTextXml(Slide slide)
    {
        var builder = new StringBuilder();
        builder.Append("<tile><visual>");
        builder.Append(SingleBinding("TileLarge", slide.Large, 2));
        builder.Append(SingleBinding("TileMedium", slide.Medium, 2));
        builder.Append(SingleBinding("TileWide", slide.Wide, 2));
        builder.Append(SingleBinding("TileSmall", slide.Small, 1));
        builder.Append("</visual></tile>");
        var document = new XmlDocument();
        document.LoadXml(builder.ToString());
        return document;
    }

    private static string SingleBinding(string template, string image, int textMode)
    {
        var builder = new StringBuilder();
        builder.Append("<binding template='").Append(template).Append("'>");
        if (!string.IsNullOrWhiteSpace(image))
        {
            builder.Append("<image src='").Append(Escape(FullPath(image))).Append("' placement='background'/>");
        }
        builder.Append(TextLines(textMode));
        builder.Append("</binding>");
        return builder.ToString();
    }

    private static string Binding(string template, List<Slide> slides, Func<Slide, string> pick, int textMode)
    {
        var builder = new StringBuilder();
        builder.Append("<binding template='").Append(template).Append("' hint-presentation='photos'>");
        foreach (Slide slide in slides)
        {
            string path = pick(slide);
            if (string.IsNullOrWhiteSpace(path))
            {
                continue;
            }
            builder.Append("<image src='").Append(Escape(FullPath(path))).Append("'/>");
        }
        builder.Append(TextLines(textMode));
        builder.Append("</binding>");
        return builder.ToString();
    }

    private static string TextLines(int mode)
    {
        var builder = new StringBuilder();
        if (mode <= 0)
        {
            return "";
        }
        if (_texts.Count > 0)
        {
            int count = mode == 1 ? 1 : _texts.Count;
            for (int index = 0; index < count; index++)
            {
                builder.Append("<text hint-style=\"").Append(_texts[index].Style).Append("\">")
                    .Append(XmlEscape(_texts[index].Text)).Append("</text>");
            }
            return builder.ToString();
        }
        if (mode == 1)
        {
            return "";
        }
        if (!string.IsNullOrWhiteSpace(_title))
        {
            builder.Append("<text hint-style=\"caption\">").Append(XmlEscape(_title)).Append("</text>");
        }
        if (!string.IsNullOrWhiteSpace(_subtitle))
        {
            builder.Append("<text hint-style=\"captionSubtle\">").Append(XmlEscape(_subtitle)).Append("</text>");
        }
        if (!string.IsNullOrWhiteSpace(_text))
        {
            builder.Append("<text hint-style=\"body\">").Append(XmlEscape(_text)).Append("</text>");
        }
        return builder.ToString();
    }

    private static string XmlEscape(string value)
    {
        return value.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
    }

    private static string FullPath(string image)
    {
        string path = image.Replace('/', '\\');
        return Path.IsPathRooted(path) ? path : Path.Combine(AppContext.BaseDirectory, path);
    }

    private static string Escape(string value)
    {
        return value.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("'", "&apos;");
    }

    private static string Str(JsonElement item, string name)
    {
        return item.TryGetProperty(name, out JsonElement value) ? (value.GetString() ?? "") : "";
    }

    private static void Log(string message)
    {
        try
        {
            File.AppendAllText(Path.Combine(Path.GetTempPath(), "koiTileLaunch.log"), DateTime.Now.ToString("HH:mm:ss.fff") + " " + message + Environment.NewLine);
        }
        catch
        {
        }
    }
}
