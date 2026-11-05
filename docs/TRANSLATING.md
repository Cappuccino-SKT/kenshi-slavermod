# Translating SlaverMod

You don't need to code or compile anything. Every language is one text file in the mod's `lang` folder:

```
Kenshi\mods\SlaverMod\lang\          (or the Workshop folder: steamapps\workshop\content\233860\3828708454\lang\)
    _template.txt   every text of the mod, untranslated: start from this one
    ru.txt          Русский
    es.txt          Español
    zh.txt          简体中文
```

English and Brazilian Portuguese are built into the mod.

## Make a new translation

1. Copy `_template.txt` and name the copy with your language code: `de.txt`, `fr.txt`, `ja.txt`, `ko.txt`, `pl.txt`...
   The code is the first part of the game's language (`de_DE` -> `de`). You can use the full code as well (`zh_cn.txt`); it wins over the short one.
2. Open it in a text editor that saves **UTF-8** (Notepad, Notepad++, VS Code...).
3. Fill in the right side of each line:

   ```
   [info]
   name = "Deutsch"          shown in Options > MCM > SlaverMod > Language
   squad = "Sklaven"         default name of the slave squad
   author = "your name"      credit

   [text]
   "You cannot enslave the last free member of your faction." = "Du kannst das letzte freie Mitglied deiner Fraktion nicht versklaven."
   " is now a slave of your faction." = " ist jetzt ein Sklave deiner Fraktion."

   [lines]
   ; You're mine now.|You'll make a good slave.|Now you work for ME.|You belong to me now.
   EnslaveMaster = "Jetzt gehörst du mir.|Du wirst ein guter Sklave.|Jetzt arbeitest du für MICH."

   [mod]
   ; Slave Hunters
   "1-SlaverMod.mod|name" = "Sklavenjäger"
   ```

   One file holds everything: the messages, the settings page, the trade window, the speech bubbles (`[lines]`) and the "Slave Hunters" game start with its characters (`[mod]`, the English text is in the comment above each line).

4. Save, start the game. With the game in your language, the mod picks the file by itself (`Language=auto` in `SlaverMod.ini`). Or choose it in *Options > MCM > SlaverMod > Language*, or write `Language=de` in `SlaverMod.ini`.

## Rules

- Only change the right side. The left side must stay exactly as it is: it is how the mod finds the text.
- **Keep the spaces at the start and at the end.** Many texts are pieces of a sentence: `" is now a slave of your faction."` follows the character's name, so it begins with a space. The name, a number or a price goes where the piece ends.
- Inside quotes, write `\"` for a quote mark and `\n` for a new line.
- An empty translation (`""`) stays in English. You can translate a little at a time and test.
- Speech `[lines]`: several lines separated by `|`; one is picked at random each time. The comment above each key shows the English lines.
- `[obsolete]` (at the end, if any) holds old texts the mod no longer uses. You can ignore them.

## Fonts

Kenshi only loads the fonts of the language the game itself is in. Chinese, Japanese, Korean or Cyrillic text shows properly when the game runs in that language; with the game in English it may show empty boxes.

## Send it

Use the **[translation form](https://github.com/Cappuccino-SKT/kenshi-slavermod/issues/new?template=translation.yml)** on GitHub (a free account is needed): write the language, drag your `.txt` file into the box and submit. No git or coding needed. A half-finished file is welcome too.

It goes into the next update with your name in `author` and on the Workshop page. Pull requests work as well, if you prefer.

## When the mod gets new texts (for the maintainer)

A file from the form: `python tools/lang/lang_tool.py merge <downloaded file> <code>` puts it in the current layout as `mod/SlaverMod/lang/<code>.txt` (the old one is kept as `.bak`), lists texts it does not know and translations that lost a leading/trailing space.

`python tools/lang/lang_tool.py update` rewrites `_template.txt` from `SlaverMod.cpp` and refreshes every language file: translations are kept, new texts are added with `""`, texts that are gone move to `[obsolete]`. `python tools/lang/lang_tool.py check` shows how much each language has.
