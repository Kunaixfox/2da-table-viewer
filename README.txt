DA TABLE VIEWER
===============

A desktop tool for Dragon Age modders to view, merge, and edit 2DA table
CSV files with full provenance tracking.


FEATURES
--------

* Family Merging - Automatically groups related files (e.g., achievements.csv,
  achievements_ep1.csv, achievements_drk.csv) and shows the merged result

* Provenance Tracking - See exactly which file provides each cell value
  (base game vs DLC vs mod)

* Search & Filter - Find families by name, filter rows by column values

* Edit & Export - Make changes and export modified source files
  (originals untouched)

* Patch System - Save edits as JSON patch files, validate before applying

* History & Undo - Track applied patches with full undo support


INSTALLATION
------------

1. Extract the ZIP to any folder
2. Run da-table-viewer.exe

No installation required.


USAGE
-----

GUI Application:

  1. Launch da-table-viewer.exe
  2. Click "Open Folder" and select your 2DA CSV directory
  3. Browse families in the left panel
  4. Click a family to view the merged table
  5. Click any cell to see its provenance (source file)
  6. Double-click a cell to edit it directly, or use the Edit tab
  7. Export or apply patches (see workflows below)


WORKFLOWS
---------

Quick Export (with pending edits):
  Edit cells -> File -> Export (Ctrl+E)
  Exports the merged table with your edits included.

Save & Share Patches:
  Edit cells -> Patch -> Save Patch (Ctrl+Shift+S)
  Creates a JSON file with your edits that can be shared.

Import Patch (preview changes):
  Patch -> Import Patch (Ctrl+Shift+I)
  Loads a patch file and shows the changes in the current view.
  You can then Export or Apply Patch.

Apply Patch (create modified files):
  Patch -> Apply Patch (Ctrl+Shift+A)
  Reads original source files, applies edits, writes new files
  to your chosen output folder. Original files are never modified.

Command-Line Tool (da-cli.exe):

  da-cli list-families --root ./2da
  da-cli show --root ./2da --family achievements
  da-cli export --root ./2da --family achievements --format csv --output out.csv
  da-cli search --root ./2da --pattern "abi"
  da-cli filter --root ./2da --family achievements --column Name --value "Hero"
  da-cli create-patch --family achievements --output patch.json
  da-cli patch --root ./2da --patch patch.json --output exports/


HOW IT WORKS
------------

Files are grouped into "families" based on their base name:

  achievements.csv      -> family "achievements" (base)
  achievements_ep1.csv  -> family "achievements" (variant: ep1)
  achievements_drk.csv  -> family "achievements" (variant: drk)

Merge Rules:

  1. Base file (no suffix) loads first
  2. Variants apply in alphabetical order by suffix
  3. Non-empty cells override previous values
  4. Empty cells preserve the base value


RECOGNIZED DLC SUFFIXES
-----------------------

drk, ep1, gib, kcc, lel, mem, shale, str, val, vala, toe, hrm, ibmoobs, gxa


PATCH FILE FORMAT
-----------------

{
  "family": "achievements",
  "edits": [
    {"row_id": 0, "column": "Name", "value": "New Name"},
    {"row_id": 5, "column": "Points", "value": "999"}
  ]
}


REQUIREMENTS
------------

* Windows 10/11 (64-bit)


