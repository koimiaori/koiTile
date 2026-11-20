using System;
using System.Collections.Generic;
using System.Text.Json;
using Windows.UI.Text;

namespace koiTile.Models
{
    internal sealed class QueueLine
    {
        public string Style { get; set; }
        public string Text { get; set; }
    }

    internal sealed class QueueLineView
    {
        public string Text { get; set; }
        public double Size { get; set; }
        public FontWeight Weight { get; set; }
    }

    internal sealed class QueueViewItem
    {
        public List<QueueLineView> Lines { get; set; }
    }

    internal static class QueueLines
    {
        private static readonly JsonSerializerOptions Options =
            new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };

        public static string Pack(IEnumerable<QueueLine> lines)
        {
            var list = new List<QueueLine>();
            foreach (QueueLine line in lines)
            {
                list.Add(new QueueLine { Style = line.Style ?? "default", Text = line.Text ?? "" });
            }
            return JsonSerializer.Serialize(list, Options);
        }

        public static List<QueueLine> Parse(string raw)
        {
            var lines = new List<QueueLine>();
            if (string.IsNullOrEmpty(raw))
            {
                return lines;
            }
            string trimmed = raw.TrimStart();
            if (trimmed.StartsWith("[", StringComparison.Ordinal))
            {
                try
                {
                    var parsed = JsonSerializer.Deserialize<List<QueueLine>>(raw, Options);
                    if (parsed != null)
                    {
                        foreach (QueueLine line in parsed)
                        {
                            if (line == null)
                            {
                                continue;
                            }
                            lines.Add(new QueueLine
                            {
                                Style = string.IsNullOrEmpty(line.Style) ? "default" : line.Style,
                                Text = line.Text ?? ""
                            });
                        }
                        return lines;
                    }
                }
                catch (JsonException)
                {
                }
            }
            foreach (string part in raw.Replace("\r", "").Split('\n'))
            {
                if (part.Length == 0)
                {
                    continue;
                }
                lines.Add(new QueueLine { Style = "default", Text = part });
            }
            return lines;
        }

        public static string Plain(string raw)
        {
            var builder = new System.Text.StringBuilder();
            foreach (QueueLine line in Parse(raw))
            {
                if (builder.Length > 0)
                {
                    builder.Append("\n");
                }
                builder.Append(line.Text);
            }
            return builder.ToString();
        }

        public static QueueViewItem View(string raw)
        {
            var item = new QueueViewItem { Lines = new List<QueueLineView>() };
            foreach (QueueLine line in Parse(raw))
            {
                item.Lines.Add(new QueueLineView
                {
                    Text = line.Text,
                    Size = SizeOf(line.Style),
                    Weight = WeightOf(line.Style)
                });
            }
            return item;
        }

        private static double SizeOf(string style)
        {
            if (string.Equals(style, "subtitle", StringComparison.Ordinal))
            {
                return 18;
            }
            if (string.Equals(style, "base", StringComparison.Ordinal))
            {
                return 16;
            }
            return 14;
        }

        private static FontWeight WeightOf(string style)
        {
            if (string.Equals(style, "captionSubtle", StringComparison.Ordinal))
            {
                return new FontWeight { Weight = 400 };
            }
            return new FontWeight { Weight = 700 };
        }
    }
}
