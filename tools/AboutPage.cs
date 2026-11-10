using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;

// The resource id varies between Vivaldi releases. Locate the template by content.
public static class VivaldiPlusAboutPage
{
    static readonly Encoding Utf8 = new UTF8Encoding(false, true);

    static byte[] Decode(byte[] bytes, int start, int length)
    {
        if (length >= 2 && bytes[start] == 0x1f && bytes[start + 1] == 0x8b)
        {
            using (var input = new MemoryStream(bytes, start, length))
            using (var gzip = new GZipStream(input, CompressionMode.Decompress))
            using (var output = new MemoryStream())
            {
                gzip.CopyTo(output);
                return output.ToArray();
            }
        }
        var result = new byte[length];
        Buffer.BlockCopy(bytes, start, result, 0, length);
        return result;
    }

    static string Escape(string text)
    {
        return text.Replace("&", "&amp;").Replace("\"", "&quot;")
            .Replace("<", "&lt;").Replace(">", "&gt;").Replace("'", "&#39;");
    }

    static int[] Validate(byte[] bytes)
    {
        if (bytes.Length < 18 || BitConverter.ToUInt32(bytes, 0) != 5)
            throw new InvalidDataException("Unsupported DataPack format (expected v5)");
        int count = BitConverter.ToUInt16(bytes, 8);
        int aliases = BitConverter.ToUInt16(bytes, 10);
        int indexEnd = checked(12 + (count + 1) * 6 + aliases * 4);
        if (indexEnd > bytes.Length) throw new InvalidDataException("Truncated resource index");
        var offsets = new int[count + 1];
        int lastId = -1;
        for (int i = 0; i <= count; i++)
        {
            int p = 12 + i * 6;
            uint offset = BitConverter.ToUInt32(bytes, p + 2);
            if (offset < indexEnd || offset > bytes.Length || (i > 0 && offset < offsets[i - 1]))
                throw new InvalidDataException("Invalid resource offsets");
            offsets[i] = checked((int)offset);
            if (i == count) break;
            int id = BitConverter.ToUInt16(bytes, p);
            if (id <= lastId) throw new InvalidDataException("Unsorted resource ids");
            lastId = id;
        }
        if (offsets[count] != bytes.Length) throw new InvalidDataException("Unexpected trailing resource bytes");
        for (int i = 0; i < aliases; i++)
            if (BitConverter.ToUInt16(bytes, 12 + (count + 1) * 6 + i * 4 + 2) >= count)
                throw new InvalidDataException("Invalid resource alias");
        return offsets;
    }

    public static byte[] Patch(byte[] bytes, string settingsUrl, string version)
    {
        var uri = new Uri(settingsUrl, UriKind.Absolute);
        if (uri.Scheme != "http" || uri.Host != "127.0.0.1" || uri.AbsolutePath != "/settings.html" || uri.Port < 16877 || uri.Port > 16887)
            throw new ArgumentException("Settings must use the Vivaldi++ loopback endpoint");
        int[] offsets = Validate(bytes);
        int selected = -1;
        string html = null;
        for (int i = 0; i < offsets.Length - 1; i++)
        {
            string text;
            try { text = Utf8.GetString(Decode(bytes, offsets[i], offsets[i + 1] - offsets[i])); }
            catch (DecoderFallbackException) { continue; }
            if (!text.Contains("about:version template page") || !text.Contains("id=\"executable_path\"") || !text.Contains("</table>")) continue;
            if (selected >= 0) throw new InvalidDataException("Multiple about:version templates; refusing to patch");
            selected = i;
            html = text;
        }
        if (selected < 0) throw new InvalidDataException("About template not found; this browser version is unsupported");
        html = Regex.Replace(html, @"\s*<!-- vivaldi-plus:start -->[\s\S]*?<!-- vivaldi-plus:end -->", "");
        string href = uri.AbsoluteUri + (uri.Query.Length == 0 ? "?" : "&") + "version=" + Uri.EscapeDataString(version);
        string row = "\n<!-- vivaldi-plus:start -->\n<tr id=\"vivaldi-plus\"><td class=\"label\">Vivaldi++</td><td class=\"version\">" + Escape(version)
            + " &middot; <a id=\"vivaldi-plus-settings\" href=\"" + Escape(href)
            + "\" target=\"_blank\" rel=\"noopener\">Settings / &#x8bbe;&#x7f6e;</a> &middot; <a href=\"https://github.com/ca-x/vivaldi_plus\" target=\"_blank\" rel=\"noopener\">GitHub</a></td></tr>\n<!-- vivaldi-plus:end -->\n";
        html = html.Replace("</table>", row + "</table>");
        byte[] body = Utf8.GetBytes(html);
        byte[] compressed;
        using (var output = new MemoryStream())
        {
            using (var gzip = new GZipStream(output, CompressionLevel.Optimal, true)) gzip.Write(body, 0, body.Length);
            compressed = output.ToArray();
        }
        int start = offsets[selected], end = offsets[selected + 1];
        int delta = compressed.Length - (end - start);
        byte[] result = new byte[checked(bytes.Length + delta)];
        Buffer.BlockCopy(bytes, 0, result, 0, start);
        Buffer.BlockCopy(compressed, 0, result, start, compressed.Length);
        Buffer.BlockCopy(bytes, end, result, start + compressed.Length, bytes.Length - end);
        for (int i = selected + 1; i < offsets.Length; i++)
            Buffer.BlockCopy(BitConverter.GetBytes(checked(offsets[i] + delta)), 0, result, 12 + i * 6 + 2, 4);
        Validate(result);
        return result;
    }

    static string JavascriptString(string text)
    {
        var result = new StringBuilder("\"");
        foreach (char c in text)
        {
            if (c == '"' || c == '\\') result.Append('\\').Append(c);
            else if (c < 32 || c == '\u2028' || c == '\u2029') result.Append("\\u").Append(((int)c).ToString("x4"));
            else result.Append(c);
        }
        return result.Append('"').ToString();
    }

    public static byte[] PatchVivaldiUi(byte[] bytes, string settingsUrl, string version)
    {
        var uri = new Uri(settingsUrl, UriKind.Absolute);
        if (uri.Scheme != "http" || uri.Host != "127.0.0.1" || uri.AbsolutePath != "/settings.html" || uri.Port < 16877 || uri.Port > 16887)
            throw new ArgumentException("Settings must use the Vivaldi++ loopback endpoint");
        string source = Utf8.GetString(bytes);
        source = Regex.Replace(source, @"/\* vivaldi-plus-ui:start \*/[\s\S]*?/\* vivaldi-plus-ui:end \*/", "");
        var pattern = new Regex(@"\(0,([A-Za-z_$][\w$]*)\.jsxs\)\(""div"",\{className:""about-page-content"",children:\[");
        var matches = pattern.Matches(source);
        if (matches.Count != 1) throw new InvalidDataException("Expected one Vivaldi about-page component; refusing to modify an unsupported UI bundle");
        var navigation = Regex.Matches(source, @"onClick:\(\)=>([A-Za-z_$][\w$]*)\(""https://vivaldi\.com/bugreport/""\)");
        if (navigation.Count != 1 || navigation[0].Index >= matches[0].Index || matches[0].Index - navigation[0].Index > 25000)
            throw new InvalidDataException("Native about-page navigation callback could not be identified");
        string navigate = navigation[0].Groups[1].Value;
        string runtime = matches[0].Groups[1].Value;
        string jsx = "(0," + runtime + ".jsx)";
        string jsxs = "(0," + runtime + ".jsxs)";
        string url = JavascriptString(uri.AbsoluteUri + (uri.Query.Length == 0 ? "?" : "&") + "version=" + Uri.EscapeDataString(version));
        string card = "/* vivaldi-plus-ui:start */" + jsxs + "(\"section\",{id:\"vivaldi-plus-about\",className:\"card helpful-links-card\",children:["
            + jsx + "(\"div\",{className:\"card-header\",children:" + jsx + "(\"h3\",{children:\"Vivaldi++\"})}),"
            + jsxs + "(\"div\",{className:\"card-content\",children:[" + jsx + "(\"p\",{children:" + JavascriptString(version) + "}),"
            + jsx + "(\"a\",{id:\"vivaldi-plus-settings\",className:\"link-item\",href:" + url
            + ",target:\"_blank\",rel:\"noopener\",onClick:e=>{e.preventDefault();" + navigate + "(" + url + ")},children:\"Settings / \\u8bbe\\u7f6e\"}),"
            + jsx + "(\"a\",{className:\"link-item\",href:\"https://github.com/ca-x/vivaldi_plus\",target:\"_blank\",rel:\"noopener\",children:\"GitHub\"})]})]}),/* vivaldi-plus-ui:end */";
        return Utf8.GetBytes(pattern.Replace(source, match => match.Value + card));
    }
}
