using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using koiTile.Interop;
using koiTile.Models;

namespace koiTile.Services
{
    internal static class TilePackage
    {
        public static bool Install(TileConfig tile)
        {
            if (tile == null)
            {
                return false;
            }

            string target;
            string arguments;
            if (!Resolve(tile, out target, out arguments))
            {
                Log.Write("package: target missing " + tile.Id);
                return false;
            }
            if (!File.Exists(Path.Combine(AppContext.BaseDirectory, "koiTileLaunch.exe")))
            {
                Log.Write("package: launcher missing");
                return false;
            }

            string root = Path.Combine(Workspace.Instance.AssetDir, "packages", tile.Id);
            Log.Write("package root: " + root);
            string assets = Path.Combine(root, "Assets");
            try
            {
                if (Directory.Exists(root))
                {
                    Directory.Delete(root, true);
                }
                Directory.CreateDirectory(assets);
                _iconFile = false;
                try
                {
                    string iconTarget = Path.Combine(assets, "icon.png");
                    string iconSource = IconSourcePath(tile);
                    if (!string.IsNullOrWhiteSpace(iconSource) && Core.kt_image_extract_icon(Core.Utf8(iconSource), 0, 256, Core.Utf8(iconTarget)) == 0)
                    {
                        _iconFile = true;
                    }
                }
                catch (Exception ex)
                {
                    Log.Write("package icon: " + ex.Message);
                }
}
            catch (Exception ex)
            {
                Log.Write("package: clean " + ex.Message);
                return false;
            }

            string wide = Path.Combine(assets, "wide.png");
            string large = Path.Combine(assets, "large.png");
            string medium = Path.Combine(assets, "medium.png");
            string small = Path.Combine(assets, "small.png");
            if (!Render(tile, 2, wide) || !Render(tile, 3, large) || !Render(tile, 1, medium) || !Render(tile, 0, small))
            {
                Log.Write("package: render failed " + tile.Id);
                return false;
            }

            try
            {
                foreach (string file in Directory.GetFiles(AppContext.BaseDirectory, "koiTileLaunch.*"))
                {
                    File.Copy(file, Path.Combine(root, Path.GetFileName(file)), true);
                }
                File.WriteAllText(Path.Combine(root, "tile.json"), BuildLaunchJson(target, arguments), new UTF8Encoding(false));
                File.WriteAllText(Path.Combine(root, "AppxManifest.xml"), BuildManifest(tile), new UTF8Encoding(false));
            }
            catch (Exception ex)
            {
                Log.Write("package: write " + ex.Message);
                return false;
            }

            try
            {
                WriteSchedule(tile, root, assets);
            }
            catch (Exception ex)
            {
                Log.Write("package schedule: " + ex.Message);
            }

            bool ok = Register(Path.Combine(root, "AppxManifest.xml"));
            if (ok)
            {
                ApplySchedule(tile.Id);
            }
            Log.Write("package " + tile.Id + ": " + (ok ? "ok" : "register failed"));
            return ok;
        }

        private static void WriteSchedule(TileConfig tile, string root, string assets)
        {
            string file = Path.Combine(root, "schedule.json");
            var slides = new List<Dictionary<string, string>>();
            if (!tile.ColorEnabled && !tile.IconEnabled)
            {
                var images = new List<string>();
                if (Workspace.Instance.Mode == 0)
                {
                    CollectImages(tile, images);
                }
                else
                {
                    foreach (TileConfig item in Workspace.Instance.Tiles)
                    {
                        CollectImages(item, images);
                    }
                }
                string current = Workspace.Instance.BackgroundImage ?? "";
                if (current.Length > 0 && File.Exists(current) && !images.Contains(current))
                {
                    images.Add(current);
                }
                for (int index = 0; index < images.Count; index++)
                {
                    string folder = Path.Combine(assets, "slide" + index);
                    Directory.CreateDirectory(folder);
                    if (!Render(tile, 0, Path.Combine(folder, "small.png"), images[index])
                        || !Render(tile, 1, Path.Combine(folder, "medium.png"), images[index])
                        || !Render(tile, 2, Path.Combine(folder, "wide.png"), images[index])
                        || !Render(tile, 3, Path.Combine(folder, "large.png"), images[index]))
                    {
                        continue;
                    }
                    slides.Add(new Dictionary<string, string>
                    {
                        ["small"] = "Assets/slide" + index + "/small.png",
                        ["medium"] = "Assets/slide" + index + "/medium.png",
                        ["wide"] = "Assets/slide" + index + "/wide.png",
                        ["large"] = "Assets/slide" + index + "/large.png"
                    });
                }
                if (slides.Count == 0 && tile.Queue.Count > 0)
                {
                    slides.Add(new Dictionary<string, string>
                    {
                        ["small"] = "",
                        ["medium"] = "",
                        ["wide"] = "",
                        ["large"] = ""
                    });
                }
            }
            if (slides.Count == 0)
            {
                if (File.Exists(file))
                {
                    File.Delete(file);
                }
                return;
            }
            var payload = new Dictionary<string, object>
            {
                ["mode"] = tile.UpdateMode,
                ["interval"] = 30,
                ["minutes"] = tile.PlanMinutes < 1 ? 30 : tile.PlanMinutes,
                ["index"] = 0,
                ["title"] = tile.Title ?? "",
                ["subtitle"] = tile.Subtitle ?? "",
                ["text"] = "",
                ["texts"] = TextPayload(tile),
                ["slides"] = slides
            };
            File.WriteAllText(file, JsonSerializer.Serialize(payload), new UTF8Encoding(false));
        }

        private static void CollectImages(TileConfig tile, List<string> images)
        {
            if (tile == null)
            {
                return;
            }
            foreach (string path in tile.Images)
            {
                if (!string.IsNullOrWhiteSpace(path) && File.Exists(path) && !images.Contains(path))
                {
                    images.Add(path);
                }
            }
        }

        private static List<Dictionary<string, string>> TextPayload(TileConfig tile)
        {
            var list = new List<Dictionary<string, string>>();
            if (tile == null || tile.Queue.Count == 0)
            {
                return list;
            }
            foreach (QueueLine line in QueueLines.Parse(tile.Queue[0]))
            {
                if (string.IsNullOrEmpty(line.Text))
                {
                    continue;
                }
                list.Add(new Dictionary<string, string>
                {
                    ["style"] = string.IsNullOrEmpty(line.Style) ? "default" : line.Style,
                    ["text"] = line.Text
                });
            }
            return list;
        }

        private static void ApplySchedule(string id)
        {
            string aumid = FindAumid(id);
            if (aumid.Length == 0)
            {
                Log.Write("schedule: no aumid for " + id);
                return;
            }
            AppActivator.Activate(aumid, "--schedule");
        }

        private static string FindAumid(string id)
        {
            try
            {
                var manager = new Windows.Management.Deployment.PackageManager();
                foreach (Windows.ApplicationModel.Package package in manager.FindPackagesForUser(string.Empty))
                {
                    if (string.Equals(package.Id.Name, "koiTile." + id, StringComparison.OrdinalIgnoreCase))
                    {
                        return package.Id.FamilyName + "!App";
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("aumid: " + ex.Message);
            }
            return "";
        }

        public static bool IsInstalled(string id)
        {
            if (string.IsNullOrWhiteSpace(id))
            {
                return false;
            }
            try
            {
                var manager = new Windows.Management.Deployment.PackageManager();
                foreach (Windows.ApplicationModel.Package package in manager.FindPackagesForUser(string.Empty))
                {
                    if (string.Equals(package.Id.Name, "koiTile." + id, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("is installed: " + ex.Message);
            }
            return false;
        }
        public static void Remove(TileConfig tile)
        {
            if (tile == null)
            {
                return;
            }
            RemoveById(tile.Id);
        }

        public static void RemoveById(string id)
        {
            if (string.IsNullOrWhiteSpace(id))
            {
                return;
            }
            string script = "$ErrorActionPreference = 'Stop'" + Environment.NewLine
                + "try {" + Environment.NewLine
                + "  Get-AppxPackage -Name 'koiTile." + id + "' | Remove-AppxPackage -ErrorAction Stop" + Environment.NewLine
                + "  exit 0" + Environment.NewLine
                + "} catch {" + Environment.NewLine
                + "  [Console]::Error.WriteLine($_.Exception.Message)" + Environment.NewLine
                + "  exit 1" + Environment.NewLine
                + "}" + Environment.NewLine;
            string uninstallScript = Path.Combine(Path.GetTempPath(), "koiTile-uninstall.ps1");
            try
            {
                File.WriteAllText(uninstallScript, script, new UTF8Encoding(true));
                Run("powershell", "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + uninstallScript + "\"");
            }
            catch (Exception ex)
            {
                Log.Write("uninstall script: " + ex.Message);
            }
            string root = Path.Combine(Workspace.Instance.AssetDir, "packages", id);
            try
            {
                if (Directory.Exists(root))
                {
                    Directory.Delete(root, true);
                }
            }
            catch (Exception ex)
            {
                Log.Write("package remove: " + ex.Message);
            }
        }

        private static bool _iconFile;

        private static bool Render(TileConfig tile, int size, string path)
        {
            return Render(tile, size, path, Workspace.Instance.BackgroundImage);
        }

        private static bool Render(TileConfig tile, int size, string path, string image)
        {
            bool hasImage = !string.IsNullOrWhiteSpace(image);
            bool iconOn = false;
            if (tile.ColorEnabled || iconOn)
            {
                bool color = tile.ColorEnabled;
                string originalIconPath = tile.IconPath ?? "";
                string iconSource = IconSourcePath(tile);
                if (!color)
                {
                    Core.kt_config_set(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("color"), Core.Utf8(WashColor()));
                    Core.kt_config_set_int(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("colorEnabled"), 1);
                }
                Core.kt_config_set_int(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("iconEnabled"), iconOn ? 1 : 0);
                if (iconOn && originalIconPath.Length == 0 && iconSource.Length > 0)
                {
                    Core.kt_config_set(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("iconPath"), Core.Utf8(iconSource));
                }
                int rc = Core.kt_image_render_tile(Workspace.Instance.Config, Core.Utf8(tile.Id), size, Core.Utf8(path));
                Core.kt_config_set_int(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("iconEnabled"), tile.IconEnabled ? 1 : 0);
                if (iconOn && originalIconPath.Length == 0)
                {
                    Core.kt_config_set(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("iconPath"), Core.Utf8(originalIconPath));
                }
                if (!color)
                {
                    Core.kt_config_set_int(Workspace.Instance.Config, Core.Utf8(tile.Id), Core.Utf8("colorEnabled"), 0);
                }
                return rc == 0;
            }
            if (hasImage)
            {
                if (!ImageSlicer.Slice(tile, size, path, image))
                {
                    return false;
                }
            }
            else
            {
                int canvasWidth = 0;
                int canvasHeight = 0;
                CanvasSizeOf(size, out canvasWidth, out canvasHeight);
                if (Core.kt_image_render_clear(canvasWidth, canvasHeight, Core.Utf8(path)) != 0)
                {
                    return false;
                }
            }
            AddGuide(tile, size, path);
            return true;
        }

        private static void AddGuide(TileConfig tile, int size, string path)
        {
            if (!Workspace.Instance.Guide || Workspace.Instance.Mode != 1 || tile == null)
            {
                return;
            }
            if (size == tile.Size)
            {
                return;
            }
            int index = Workspace.Instance.GuideIndex(tile);
            if (index <= 0)
            {
                return;
            }
            int canvasWidth = 0;
            int canvasHeight = 0;
            CanvasSizeOf(size, out canvasWidth, out canvasHeight);
            int fontPx = Math.Max(10, canvasHeight / 6);
            Core.kt_image_add_label_box(Core.Utf8(path), Core.Utf8(Workspace.SizeLabel(tile.Size)), Core.Utf8(index.ToString("00")), fontPx, Core.Utf8(path));
        }

        private static string IconSourcePath(TileConfig tile)
        {
            string path = string.IsNullOrWhiteSpace(tile.IconPath) ? (tile.Shortcut ?? "") : tile.IconPath;
            if (string.IsNullOrWhiteSpace(path))
            {
                return "";
            }
            if (path.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase))
            {
                path = Core.ShortcutTarget(path);
            }
            return File.Exists(path) ? path : "";
        }

        private static bool IconAvailable(TileConfig tile)
        {
            return IconSourcePath(tile).Length > 0;
        }

        private static void CanvasSizeOf(int size, out int width, out int height)
        {
            if (size == 0)
            {
                width = 70;
                height = 70;
                return;
            }
            if (size == 1)
            {
                width = 150;
                height = 150;
                return;
            }
            if (size == 2)
            {
                width = 310;
                height = 150;
                return;
            }
            width = 310;
            height = 310;
        }

        internal static bool DarkTheme()
        {
            try
            {
                using (Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"))
                {
                    object value = key == null ? null : key.GetValue("AppsUseLightTheme");
                    return value is int light && light == 0;
                }
            }
            catch (Exception ex)
            {
                Log.Write("theme: " + ex.Message);
                return true;
            }
        }

        internal static string WashColor()
        {
            int r = 31;
            int g = 31;
            int b = 31;
            if (!DarkTheme())
            {
                r = 228;
                g = 228;
                b = 228;
            }
            double alpha = DarkTheme() ? 0.1 : 0.4;
            r = (int)Math.Round(r + (255 - r) * alpha);
            g = (int)Math.Round(g + (255 - g) * alpha);
            b = (int)Math.Round(b + (255 - b) * alpha);
            return "#" + r.ToString("X2") + g.ToString("X2") + b.ToString("X2");
        }

        private static bool Resolve(TileConfig tile, out string target, out string arguments)
        {
            target = "";
            arguments = "";
            string value = tile.Shortcut ?? "";
            if (string.IsNullOrWhiteSpace(value))
            {
                return true;
            }
            if (tile.LaunchType == 3)
            {
                target = "cmd.exe";
                arguments = "/c " + value;
                return true;
            }
            if (tile.LaunchType == 1)
            {
                target = value;
                return true;
            }
            if (tile.LaunchType == 2)
            {
                target = value;
                return File.Exists(target) || Directory.Exists(target);
            }
            target = value.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase) ? Core.ShortcutTarget(value) : value;
            return target.Length > 0 && (File.Exists(target) || Directory.Exists(target));
        }

        private static string BuildLaunchJson(string target, string arguments)
        {
            var builder = new StringBuilder();
            builder.Append("{\"target\":").Append(JsonSerializer.Serialize(target));
            builder.Append(",\"args\":").Append(JsonSerializer.Serialize(arguments));
            builder.Append(",\"lib\":").Append(JsonSerializer.Serialize(AppContext.BaseDirectory));
            builder.Append("}");
            return builder.ToString();
        }

        private static string BuildManifest(TileConfig tile)
        {
            string name = "koiTile." + tile.Id;
            string display = Escape(Workspace.Instance.TileLabel(tile));
            bool iconTiles = tile.IconEnabled && _iconFile;
            var builder = new StringBuilder();
            builder.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
            builder.AppendLine("<Package");
            builder.AppendLine("  xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\"");
            builder.AppendLine("  xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\"");
            builder.AppendLine("  xmlns:rescap=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities\"");
            builder.AppendLine("  IgnorableNamespaces=\"uap rescap\">");
            DateTime now = DateTime.Now;
            int build = (int)(now.Date - new DateTime(2020, 1, 1)).TotalDays;
            int revision = (now.Hour * 60) + now.Minute;
            builder.AppendLine("  <Identity Name=\"" + name + "\" Publisher=\"CN=koiTile\" Version=\"1.0." + build + "." + revision + "\" ProcessorArchitecture=\"x64\" />");
            builder.AppendLine("  <Properties>");
            builder.AppendLine("    <DisplayName>" + display + "</DisplayName>");
            builder.AppendLine("    <PublisherDisplayName>koiTile</PublisherDisplayName>");
            builder.AppendLine("    <Logo>" + (_iconFile ? "Assets\\icon.png" : "Assets\\small.png") + "</Logo>");
            builder.AppendLine("  </Properties>");
            builder.AppendLine("  <Dependencies>");
            builder.AppendLine("    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.19041.0\" MaxVersionTested=\"10.0.26100.0\" />");
            builder.AppendLine("  </Dependencies>");
            builder.AppendLine("  <Resources>");
            builder.AppendLine("    <Resource Language=\"zh-CN\" />");
            builder.AppendLine("  </Resources>");
            builder.AppendLine("  <Applications>");
            builder.AppendLine("    <Application Id=\"App\" Executable=\"koiTileLaunch.exe\" EntryPoint=\"Windows.FullTrustApplication\">");
            builder.AppendLine("      <uap:VisualElements");
            builder.AppendLine("        DisplayName=\"" + display + "\"");
            builder.AppendLine("        Description=\"koiTile\"");
            builder.AppendLine("        BackgroundColor=\"" + (DarkTheme() ? "#1F1F1F" : "#E4E4E4") + "\"");
            builder.AppendLine("        Square44x44Logo=\"" + (_iconFile ? "Assets\\icon.png" : "Assets\\small.png") + "\"");
            builder.AppendLine("        Square150x150Logo=\"" + (iconTiles ? "Assets\\icon.png" : "Assets\\medium.png") + "\">");
            if (iconTiles)
            {
            }
            else if (tile.ShowName)
            {
                builder.AppendLine("        <uap:DefaultTile Wide310x150Logo=\"Assets\\wide.png\" Square310x310Logo=\"Assets\\large.png\">");
                builder.AppendLine("          <uap:ShowNameOnTiles>");
                builder.AppendLine("            <uap:ShowOn Tile=\"square150x150Logo\" />");
                builder.AppendLine("            <uap:ShowOn Tile=\"wide310x150Logo\" />");
                builder.AppendLine("            <uap:ShowOn Tile=\"square310x310Logo\" />");
                builder.AppendLine("          </uap:ShowNameOnTiles>");
                builder.AppendLine("        </uap:DefaultTile>");
            }
            else
            {
                builder.AppendLine("        <uap:DefaultTile Wide310x150Logo=\"Assets\\wide.png\" Square310x310Logo=\"Assets\\large.png\" />");
            }
            builder.AppendLine("      </uap:VisualElements>");
            builder.AppendLine("    </Application>");
            builder.AppendLine("  </Applications>");
            builder.AppendLine("  <Capabilities>");
            builder.AppendLine("    <rescap:Capability Name=\"runFullTrust\" />");
            builder.AppendLine("  </Capabilities>");
            builder.AppendLine("</Package>");
            return builder.ToString();
        }

        private static string Escape(string value)
        {
            return value.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
        }

        private static bool Register(string manifest)
        {
            string script = "$ErrorActionPreference = 'Stop'" + Environment.NewLine
                + "try {" + Environment.NewLine
                + "  Add-AppxPackage -Register '" + manifest.Replace("'", "''") + "' -ErrorAction Stop" + Environment.NewLine
                + "  exit 0" + Environment.NewLine
                + "} catch {" + Environment.NewLine
                + "  [Console]::Error.WriteLine($_.Exception.Message)" + Environment.NewLine
                + "  exit 1" + Environment.NewLine
                + "}" + Environment.NewLine;
            string file = Path.Combine(Path.GetTempPath(), "koiTile-register.ps1");
            try
            {
                File.WriteAllText(file, script, new UTF8Encoding(true));
            }
            catch (Exception ex)
            {
                Log.Write("register script: " + ex.Message);
                return false;
            }
            return Run("powershell", "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + file + "\"");
        }

        private static bool Run(string file, string arguments)
        {
            try
            {
                var info = new ProcessStartInfo(file, arguments)
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };
                using (Process process = Process.Start(info))
                {
                    if (process == null)
                    {
                        return false;
                    }
                    string output = process.StandardOutput.ReadToEnd();
                    string error = process.StandardError.ReadToEnd();
                    process.WaitForExit();
                    if (process.ExitCode != 0)
                    {
                        Log.Write("run " + Path.GetFileName(file) + " failed: " + output.Trim() + error.Trim());
                        return false;
                    }
                    return true;
                }
            }
            catch (Exception ex)
            {
                Log.Write("run " + Path.GetFileName(file) + ": " + ex.Message);
                return false;
            }
        }
    }
}
