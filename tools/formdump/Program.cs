using System;
using System.Collections.Generic;
// Dumps PKHeX's own form lists so PKSE can generate a table from them rather than
// re-implementing FormConverter. Output is JSON on stdout: { lang: { species: [names...] } }.
using System.Text;
using System.Text.Json;
using PKHeX.Core;

const ushort MaxSpecies = 1025;
string[] languages = ["ja", "en", "fr", "it", "de", "es", "ko", "zh-Hans", "zh-Hant"];

var result = new Dictionary<string, Dictionary<string, string[]>>();
foreach (var lang in languages)
{
    var strings = GameInfo.GetStrings(lang);
    var perSpecies = new Dictionary<string, string[]>();
    // UNION ACROSS CONTEXTS. PKSE names every form a buffer can hold, including ones a later
    // generation dropped -- Raticate's Totem form exists only in the Gen 7 list, and asking Gen 9
    // alone would silently lose it. Merge per index, newest context first so current wording wins.
    EntityContext[] contexts =
    [
        EntityContext.Gen9, EntityContext.Gen9a, EntityContext.Gen8, EntityContext.Gen8a,
        EntityContext.Gen8b, EntityContext.Gen7, EntityContext.Gen7b, EntityContext.Gen6,
        EntityContext.Gen5, EntityContext.Gen4, EntityContext.Gen3,
    ];
    for (ushort species = 1; species <= MaxSpecies; species++)
    {
        var merged = new List<string>();
        foreach (var context in contexts)
        {
            string[] forms;
            try { forms = FormConverter.GetFormList(species, strings.types, strings.forms,
                                                    GameInfo.GenderSymbolUnicode, context); }
            catch { continue; }
            for (int i = 0; i < forms.Length; i++)
            {
                if (i == merged.Count) merged.Add(forms[i]);
                else if (string.IsNullOrEmpty(merged[i])) merged[i] = forms[i];
            }
        }
        if (merged.Count > 1)
            perSpecies[species.ToString()] = merged.ToArray();
    }
    result[lang] = perSpecies;
}
Console.OutputEncoding = Encoding.UTF8;
Console.WriteLine(JsonSerializer.Serialize(result, new JsonSerializerOptions { WriteIndented = false }));
