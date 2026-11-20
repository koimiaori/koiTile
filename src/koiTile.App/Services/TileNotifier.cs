using System;
using System.Collections.Generic;
using System.IO;
using koiTile.Interop;
using koiTile.Models;
using Windows.Data.Xml.Dom;
using Windows.UI.Notifications;

namespace koiTile.Services
{
    internal static class TileNotifier
    {
        private const int MaxNotifications = 5;
        private static bool? _supported;

        public static string EnsureAumid(TileConfig tile)
        {
            if (tile == null || string.IsNullOrWhiteSpace(tile.Shortcut) || !File.Exists(tile.Shortcut))
            {
                return string.Empty;
            }

            string aumid = Core.Get((buffer, length) => Core.kt_shortcut_aumid(Core.Utf8(tile.Shortcut), buffer, length), 512);
            if (!string.IsNullOrEmpty(aumid))
            {
                return aumid;
            }

            aumid = "koiTile." + tile.Id;
            if (Core.kt_shortcut_create(Core.Utf8(""), Core.Utf8(tile.Shortcut), Core.Utf8(aumid), Core.Utf8(""), Core.Utf8("")) != 0)
            {
                Log.Write("aumid failed: " + Core.LastError());
                return string.Empty;
            }
            return aumid;
        }

        public static bool NeedsDynamic(TileConfig tile)
        {
            if (tile == null)
            {
                return false;
            }
            return tile.TextSource != 0 || tile.Queue.Count > 0 || tile.Peek.Count > 1;
        }

        public static bool Push(TileConfig tile, string content, string image)
        {
            return Push(tile, content, image, false);
        }

        public static bool Push(TileConfig tile, string content, string image, bool force)
        {
            if (!force && !NeedsDynamic(tile))
            {
                return false;
            }
            if (_supported == null)
            {
                _supported = Core.kt_env_tile_notifications() != 0;
            }
            if (_supported != true)
            {
                return false;
            }

            string aumid = EnsureAumid(tile);
            if (string.IsNullOrEmpty(aumid))
            {
                return false;
            }

            try
            {
                TileUpdater updater = TileUpdateManager.CreateTileUpdaterForApplication(aumid);
                updater.EnableNotificationQueue(true);
                updater.Clear();

                List<string> frames = BuildFrames(tile, content, image);
                int count = Math.Min(frames.Count, MaxNotifications);
                for (int i = 0; i < count; i++)
                {
                    var document = new XmlDocument();
                    document.LoadXml(frames[i]);
                    var notification = new TileNotification(document);
                    notification.Tag = "koiTile" + i.ToString();
                    updater.Update(notification);
                }
                return true;
            }
            catch (Exception ex)
            {
                Log.Write("tile push failed: " + ex.Message);
                return false;
            }
        }

        private static List<string> BuildFrames(TileConfig tile, string content, string image)
        {
            var frames = new List<string>();
            string headline = FirstLine(content);
            string mainImage = ResolveTileImage(tile, image);
            if (tile.Queue.Count > 0)
            {
                for (int i = 0; i < tile.Queue.Count && frames.Count < MaxNotifications; i++)
                {
                    frames.Add(BuildXml(tile, QueueLines.Parse(tile.Queue[i]), "", mainImage));
                }
                return frames;
            }
            frames.Add(BuildXml(tile, null, headline, mainImage));
            if (tile.Peek.Count > 1)
            {
                for (int i = 0; i < tile.Peek.Count && frames.Count < MaxNotifications; i++)
                {
                    frames.Add(BuildXml(tile, null, headline, ResolveImage(tile.Peek[i])));
                }
            }
            return frames;
        }

        private static string ResolveTileImage(TileConfig tile, string image)
        {
            if (!string.IsNullOrWhiteSpace(image))
            {
                return ResolveImage(image);
            }
            return "";
        }

        private static string ResolveImage(string path)
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                return "";
            }
            try
            {
                return new Uri(path).AbsoluteUri;
            }
            catch
            {
                return "";
            }
        }

        private static string FirstLine(string content)
        {
            if (string.IsNullOrEmpty(content))
            {
                return "";
            }
            int index = content.IndexOfAny(new[] { '\r', '\n' });
            string line = index < 0 ? content : content.Substring(0, index);
            return line.Length > 120 ? line.Substring(0, 120) : line;
        }

        public static string PreviewXml(TileConfig tile)
        {
            if (tile == null)
            {
                return "";
            }
            return BuildXml(tile, QueueLines.Parse(tile.Queue.Count > 0 ? tile.Queue[0] : ""), "", "");
        }

        private static string BuildXml(TileConfig tile, List<QueueLine> lines, string text, string image)
        {
            string displayName = Escape(tile.ShowName ? tile.AppName : "");
            string imageTag = string.IsNullOrEmpty(image) ? "" : "<image src=\"" + image + "\" placement=\"background\"/>";
            bool hasLines = lines != null && lines.Count > 0;
            string title = tile.Title ?? "";
            string subtitle = tile.Subtitle ?? "";

            string xml = "<tile><visual version=\"3\" branding=\"none\" displayName=\"" + displayName + "\">";
            xml += "<binding template=\"TileSmall\" branding=\"none\" displayName=\"" + displayName + "\">" + imageTag +
                (hasLines ? Text(lines[0].Style, lines[0].Text) : Text("caption", title)) + "</binding>";
            xml += "<binding template=\"TileMedium\" branding=\"none\" displayName=\"" + displayName + "\">" + imageTag +
                (hasLines ? Lines(lines) + Text("captionSubtle", text) : Text("caption", title) + Text("captionSubtle", subtitle) + Text("captionSubtle", text)) + "</binding>";
            xml += "<binding template=\"TileWide\" branding=\"none\" displayName=\"" + displayName + "\">" + imageTag +
                (hasLines ? Lines(lines) + Text("body", text) : Text("caption", title) + Text("captionSubtle", subtitle) + Text("body", text)) + "</binding>";
            xml += "<binding template=\"TileLarge\" branding=\"none\" displayName=\"" + displayName + "\">" + imageTag +
                (hasLines ? Lines(lines) + Text("body", text) : Text("caption", title) + Text("captionSubtle", subtitle) + Text("body", text)) + "</binding>";
            xml += "</visual></tile>";
            return xml;
        }

        private static string Text(string style, string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                return "";
            }
            return "<text hint-style=\"" + style + "\">" + Escape(value) + "</text>";
        }

        private static string Lines(List<QueueLine> lines)
        {
            var builder = new System.Text.StringBuilder();
            foreach (QueueLine line in lines)
            {
                builder.Append(Text(line.Style, line.Text));
            }
            return builder.ToString();
        }

        private static string Escape(string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                return "";
            }
            return value.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;").Replace("'", "&apos;");
        }
    }
}
