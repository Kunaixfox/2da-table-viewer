//! DA Table Viewer CLI
//!
//! Command-line tool for scanning, viewing, and exporting Dragon Age 2DA tables.

use clap::{Parser, Subcommand};
use da_core::{
    apply_patch, create_history_entry, export_with_edits, merge_family, parse_csv, scan_directory,
    BatchFile, Edit, HistoryFile, PatchFile,
};
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::PathBuf;

#[derive(Parser)]
#[command(name = "da-cli")]
#[command(about = "Dragon Age 2DA Table Viewer", long_about = None)]
#[command(version)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Scan directories for CSV files and index families
    Scan {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,
    },

    /// List all discovered families
    ListFamilies {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Show member files for each family
        #[arg(short, long)]
        verbose: bool,
    },

    /// Show a merged table
    Show {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Family name to show
        #[arg(short, long)]
        family: String,

        /// Maximum number of rows to display
        #[arg(short, long)]
        limit: Option<usize>,

        /// Columns to display (comma-separated)
        #[arg(short, long)]
        columns: Option<String>,
    },

    /// Export a merged table to a file
    Export {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Family name to export
        #[arg(short, long)]
        family: String,

        /// Output format (csv or json)
        #[arg(long, default_value = "csv")]
        format: String,

        /// Output file path
        #[arg(short, long)]
        output: PathBuf,
    },

    /// Explain the provenance of a specific cell
    Explain {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Family name
        #[arg(short, long)]
        family: String,

        /// Row ID
        #[arg(long)]
        row: i64,

        /// Column name
        #[arg(long)]
        col: String,
    },

    /// Parse and display a single CSV file
    Parse {
        /// Path to CSV file
        #[arg(short, long)]
        file: PathBuf,
    },

    /// Apply a patch file and export modified source files
    Patch {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Path to patch file (JSON)
        #[arg(short, long)]
        patch: PathBuf,

        /// Output directory for modified files
        #[arg(short, long)]
        output: PathBuf,
    },

    /// Run a batch of patch operations
    Batch {
        /// Path to batch file (JSON)
        #[arg(short, long)]
        batch: PathBuf,
    },

    /// Create an empty patch file template
    CreatePatch {
        /// Family name for the patch
        #[arg(short, long)]
        family: String,

        /// Output path for the patch file
        #[arg(short, long)]
        output: PathBuf,

        /// Example edits to include (row_id:column:value)
        #[arg(short, long)]
        example: Vec<String>,
    },

    /// Create an empty batch file template
    CreateBatch {
        /// Output path for the batch file
        #[arg(short, long)]
        output: PathBuf,

        /// Root directories to include
        #[arg(short, long)]
        root: Vec<PathBuf>,

        /// Output directory for exports
        #[arg(long)]
        export_dir: PathBuf,
    },

    /// Search for families by name pattern
    Search {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Search pattern (substring match, case-insensitive)
        #[arg(short, long)]
        pattern: String,
    },

    /// Filter rows in a family by column value
    Filter {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Family name
        #[arg(short, long)]
        family: String,

        /// Column to filter on
        #[arg(short, long)]
        column: String,

        /// Value to search for (substring match, case-insensitive)
        #[arg(short, long)]
        value: String,

        /// Maximum rows to display
        #[arg(short, long)]
        limit: Option<usize>,
    },

    /// Validate a patch file without applying it
    Validate {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Path to patch file (JSON)
        #[arg(short, long)]
        patch: PathBuf,
    },

    /// Show patch history for a family
    History {
        /// Path to history file
        #[arg(short = 'H', long, default_value = ".da-history.json")]
        history_file: PathBuf,

        /// Family name (optional, show all if not specified)
        #[arg(short, long)]
        family: Option<String>,
    },

    /// Undo the last patch applied to a family
    Undo {
        /// Root directories to scan
        #[arg(short, long, required = true)]
        root: Vec<PathBuf>,

        /// Path to history file
        #[arg(short = 'H', long, default_value = ".da-history.json")]
        history_file: PathBuf,

        /// Family name
        #[arg(short, long)]
        family: String,

        /// Output directory for restored files
        #[arg(short, long)]
        output: PathBuf,
    },
}

fn main() {
    if let Err(e) = run() {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}

fn run() -> da_core::Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Commands::Scan { root } => cmd_scan(&root),
        Commands::ListFamilies { root, verbose } => cmd_list_families(&root, verbose),
        Commands::Show {
            root,
            family,
            limit,
            columns,
        } => cmd_show(&root, &family, limit, columns),
        Commands::Export {
            root,
            family,
            format,
            output,
        } => cmd_export(&root, &family, &format, &output),
        Commands::Explain {
            root,
            family,
            row,
            col,
        } => cmd_explain(&root, &family, row, &col),
        Commands::Parse { file } => cmd_parse(&file),
        Commands::Patch { root, patch, output } => cmd_patch(&root, &patch, &output, None),
        Commands::Batch { batch } => cmd_batch(&batch),
        Commands::CreatePatch { family, output, example } => cmd_create_patch(&family, &output, &example),
        Commands::CreateBatch { output, root, export_dir } => cmd_create_batch(&output, &root, &export_dir),
        Commands::Search { root, pattern } => cmd_search(&root, &pattern),
        Commands::Filter { root, family, column, value, limit } => cmd_filter(&root, &family, &column, &value, limit),
        Commands::Validate { root, patch } => cmd_validate(&root, &patch),
        Commands::History { history_file, family } => cmd_history(&history_file, family.as_deref()),
        Commands::Undo { root, history_file, family, output } => cmd_undo(&root, &history_file, &family, &output),
    }
}

fn cmd_scan(roots: &[PathBuf]) -> da_core::Result<()> {
    let result = scan_directory(roots)?;

    println!("Scanned {} root(s):", result.roots.len());
    for root in &result.roots {
        println!("  {}", root.display());
    }
    println!();
    println!("Found {} files in {} families", result.total_files, result.families.len());

    Ok(())
}

fn cmd_list_families(roots: &[PathBuf], verbose: bool) -> da_core::Result<()> {
    let result = scan_directory(roots)?;

    println!("Families ({}):", result.families.len());
    println!();

    for family in &result.families {
        if verbose {
            println!("{} ({} files)", family.name, family.members.len());
            for member in &family.members {
                let suffix_str = match &member.suffix {
                    Some(s) => format!(" [{}]", s),
                    None => " [base]".to_string(),
                };
                println!("  {}{}", member.path.display(), suffix_str);
            }
            println!();
        } else {
            println!("  {} ({} files)", family.name, family.members.len());
        }
    }

    Ok(())
}

fn cmd_show(
    roots: &[PathBuf],
    family_name: &str,
    limit: Option<usize>,
    columns: Option<String>,
) -> da_core::Result<()> {
    let scan_result = scan_directory(roots)?;

    let family = scan_result
        .find_family(family_name)
        .ok_or_else(|| da_core::Error::FamilyNotFound(family_name.to_string()))?;

    let merged = merge_family(family)?;

    // Filter columns if specified
    let col_filter: Option<Vec<&str>> = columns.as_ref().map(|c| c.split(',').collect());

    let display_cols: Vec<&da_core::Column> = if let Some(ref filter) = col_filter {
        merged
            .columns
            .iter()
            .filter(|c| filter.contains(&c.name.as_str()))
            .collect()
    } else {
        merged.columns.iter().collect()
    };

    // Print header
    let header: Vec<&str> = display_cols.iter().map(|c| c.name.as_str()).collect();
    println!("{}", header.join("\t"));
    println!("{}", "-".repeat(header.len() * 12));

    // Print rows
    let row_limit = limit.unwrap_or(merged.rows.len());
    for row in merged.rows.iter().take(row_limit) {
        let values: Vec<String> = display_cols
            .iter()
            .map(|col| {
                row.cells
                    .get(col.index)
                    .map(|c| c.value.to_string_value())
                    .unwrap_or_default()
            })
            .collect();
        println!("{}", values.join("\t"));
    }

    if merged.rows.len() > row_limit {
        println!("... ({} more rows)", merged.rows.len() - row_limit);
    }

    Ok(())
}

fn cmd_export(
    roots: &[PathBuf],
    family_name: &str,
    format: &str,
    output: &PathBuf,
) -> da_core::Result<()> {
    let scan_result = scan_directory(roots)?;

    let family = scan_result
        .find_family(family_name)
        .ok_or_else(|| da_core::Error::FamilyNotFound(family_name.to_string()))?;

    let merged = merge_family(family)?;

    let file = File::create(output)?;
    let mut writer = BufWriter::new(file);

    match format.to_lowercase().as_str() {
        "csv" => {
            // Write header
            let header: Vec<&str> = merged.columns.iter().map(|c| c.name.as_str()).collect();
            writeln!(writer, "{}", header.join(","))?;

            // Write rows
            for row in &merged.rows {
                let values: Vec<String> = row
                    .cells
                    .iter()
                    .map(|c| escape_csv(&c.value.to_string_value()))
                    .collect();
                writeln!(writer, "{}", values.join(","))?;
            }
        }
        "json" => {
            let json = serde_json::to_string_pretty(&merged)?;
            writeln!(writer, "{}", json)?;
        }
        _ => {
            eprintln!("Unknown format: {}. Supported formats: csv, json", format);
            std::process::exit(1);
        }
    }

    println!("Exported {} rows to {}", merged.rows.len(), output.display());

    Ok(())
}

fn cmd_explain(roots: &[PathBuf], family_name: &str, row_id: i64, col_name: &str) -> da_core::Result<()> {
    let scan_result = scan_directory(roots)?;

    let family = scan_result
        .find_family(family_name)
        .ok_or_else(|| da_core::Error::FamilyNotFound(family_name.to_string()))?;

    let merged = merge_family(family)?;

    let col = merged
        .find_column(col_name)
        .ok_or_else(|| da_core::Error::InvalidFamilyName(format!("column '{}' not found", col_name)))?;

    let row = merged
        .find_row(row_id)
        .ok_or_else(|| da_core::Error::InvalidFamilyName(format!("row ID {} not found", row_id)))?;

    let cell = &row.cells[col.index];

    println!("Family: {}", family_name);
    println!("Row ID: {}", row_id);
    println!("Column: {}", col_name);
    println!();
    println!("Value: {}", cell.value);
    println!("Source: {}", cell.source.display());
    println!();
    println!("Contributing files (merge order):");
    for (i, source) in merged.sources.iter().enumerate() {
        let marker = if source == &cell.source { " <-- winner" } else { "" };
        println!("  {}. {}{}", i + 1, source.display(), marker);
    }

    Ok(())
}

fn cmd_parse(file: &PathBuf) -> da_core::Result<()> {
    let table = parse_csv(file)?;

    println!("File: {}", file.display());
    println!("Columns: {}", table.column_count());
    println!("Rows: {}", table.row_count());
    println!();

    // Print header
    let header: Vec<&str> = table.columns.iter().map(|c| c.name.as_str()).collect();
    println!("{}", header.join("\t"));
    println!("{}", "-".repeat(header.len() * 12));

    // Print first 10 rows
    for row in table.rows.iter().take(10) {
        let values: Vec<String> = row.cells.iter().map(|c| c.to_string_value()).collect();
        println!("{}", values.join("\t"));
    }

    if table.row_count() > 10 {
        println!("... ({} more rows)", table.row_count() - 10);
    }

    Ok(())
}

fn cmd_patch(
    roots: &[PathBuf],
    patch_path: &PathBuf,
    output_dir: &PathBuf,
    history_file: Option<&PathBuf>,
) -> da_core::Result<()> {
    // Load the patch file
    let patch = PatchFile::load(patch_path)?;
    println!("Loaded patch for family '{}' with {} edits", patch.family, patch.edits.len());

    // Scan and find the family
    let scan_result = scan_directory(roots)?;
    let family = scan_result
        .find_family(&patch.family)
        .ok_or_else(|| da_core::Error::FamilyNotFound(patch.family.clone()))?;

    // Merge the family
    let merged = merge_family(family)?;
    println!("Merged {} rows from {} source files", merged.rows.len(), merged.sources.len());

    // Preview which files will be affected
    let preview = apply_patch(&merged, &patch)?;

    if !preview.failed_edits.is_empty() {
        println!("\nWarning: {} edits could not be applied:", preview.failed_edits.len());
        for (edit, reason) in &preview.failed_edits {
            println!("  - Row {}, Column '{}': {}", edit.row_id, edit.column, reason);
        }
    }

    if preview.modified_sources.is_empty() {
        println!("\nNo files to modify.");
        return Ok(());
    }

    println!("\nFiles to be modified:");
    for (source, row_ids) in &preview.modified_sources {
        println!("  {} ({} edits)", source.display(), row_ids.len());
    }

    // Export with edits
    let result = export_with_edits(&merged, &patch, output_dir)?;

    println!("\nExport complete:");
    println!("  {} files written to {}", result.files_written.len(), output_dir.display());
    println!("  {} edits applied", result.edits_applied);

    for path in &result.files_written {
        println!("  - {}", path.display());
    }

    if !result.errors.is_empty() {
        println!("\nErrors:");
        for (path, err) in &result.errors {
            println!("  {}: {}", path.display(), err);
        }
    }

    // Record in history if history file specified
    if let Some(hist_path) = history_file {
        let mut history = HistoryFile::load(hist_path)?;
        let entry = create_history_entry(&patch, result.files_written, output_dir.clone());
        history.add_entry(entry);
        history.save(hist_path)?;
        println!("\nRecorded in history: {}", hist_path.display());
    }

    Ok(())
}

fn cmd_batch(batch_path: &PathBuf) -> da_core::Result<()> {
    let batch = BatchFile::load(batch_path)?;

    println!("Running batch with {} patch files", batch.patches.len());
    println!("Roots: {:?}", batch.roots);
    println!("Output: {}", batch.output_dir.display());
    println!();

    // Scan once for all patches
    let scan_result = scan_directory(&batch.roots)?;

    let mut total_edits = 0;
    let mut total_files = 0;
    let mut errors = Vec::new();

    for patch_path in &batch.patches {
        println!("Processing patch: {}", patch_path.display());

        let patch = match PatchFile::load(patch_path) {
            Ok(p) => p,
            Err(e) => {
                errors.push((patch_path.clone(), e.to_string()));
                continue;
            }
        };

        let family = match scan_result.find_family(&patch.family) {
            Some(f) => f,
            None => {
                errors.push((patch_path.clone(), format!("Family '{}' not found", patch.family)));
                continue;
            }
        };

        let merged = match merge_family(family) {
            Ok(m) => m,
            Err(e) => {
                errors.push((patch_path.clone(), e.to_string()));
                continue;
            }
        };

        match export_with_edits(&merged, &patch, &batch.output_dir) {
            Ok(result) => {
                total_edits += result.edits_applied;
                total_files += result.files_written.len();
                println!("  Applied {} edits, wrote {} files", result.edits_applied, result.files_written.len());
            }
            Err(e) => {
                errors.push((patch_path.clone(), e.to_string()));
            }
        }
    }

    println!();
    println!("Batch complete:");
    println!("  {} total edits applied", total_edits);
    println!("  {} total files written", total_files);

    if !errors.is_empty() {
        println!("\nErrors ({}):", errors.len());
        for (path, err) in &errors {
            println!("  {}: {}", path.display(), err);
        }
    }

    Ok(())
}

fn cmd_create_patch(family: &str, output: &PathBuf, examples: &[String]) -> da_core::Result<()> {
    let mut patch = PatchFile::new(family);

    // Parse example edits: "row_id:column:value"
    for example in examples {
        let parts: Vec<&str> = example.splitn(3, ':').collect();
        if parts.len() != 3 {
            eprintln!("Warning: Invalid example format '{}', expected 'row_id:column:value'", example);
            continue;
        }

        let row_id: i64 = match parts[0].parse() {
            Ok(id) => id,
            Err(_) => {
                eprintln!("Warning: Invalid row_id '{}' in example", parts[0]);
                continue;
            }
        };

        patch.add_edit(Edit::new(row_id, parts[1], parts[2]));
    }

    // If no examples provided, add a placeholder
    if patch.edits.is_empty() {
        patch.add_edit(Edit::new(0, "ColumnName", "NewValue"));
    }

    patch.save(output)?;
    println!("Created patch file: {}", output.display());
    println!("Family: {}", family);
    println!("Edits: {}", patch.edits.len());
    println!();
    println!("Edit the file to add your changes, then run:");
    println!("  da-cli patch --root <path> --patch {} --output <dir>", output.display());

    Ok(())
}

fn cmd_create_batch(output: &PathBuf, roots: &[PathBuf], export_dir: &PathBuf) -> da_core::Result<()> {
    let batch = BatchFile {
        roots: roots.to_vec(),
        output_dir: export_dir.clone(),
        patches: vec![PathBuf::from("patch1.json"), PathBuf::from("patch2.json")],
    };

    batch.save(output)?;
    println!("Created batch file: {}", output.display());
    println!();
    println!("Edit the file to configure your batch, then run:");
    println!("  da-cli batch --batch {}", output.display());

    Ok(())
}

/// Escape a value for CSV output
fn escape_csv(s: &str) -> String {
    if s.contains(',') || s.contains('"') || s.contains('\n') {
        format!("\"{}\"", s.replace('"', "\"\""))
    } else {
        s.to_string()
    }
}

fn cmd_search(roots: &[PathBuf], pattern: &str) -> da_core::Result<()> {
    let scan_result = scan_directory(roots)?;
    let pattern_lower = pattern.to_lowercase();

    let matches: Vec<_> = scan_result
        .families
        .iter()
        .filter(|f| f.name.to_lowercase().contains(&pattern_lower))
        .collect();

    if matches.is_empty() {
        println!("No families found matching '{}'", pattern);
        return Ok(());
    }

    println!("Found {} families matching '{}':\n", matches.len(), pattern);

    for family in matches {
        println!("{} ({} files)", family.name, family.members.len());
        for member in &family.members {
            let suffix_str = match &member.suffix {
                Some(s) => format!(" [{}]", s),
                None => " [base]".to_string(),
            };
            println!("  {}{}", member.path.display(), suffix_str);
        }
        println!();
    }

    Ok(())
}

fn cmd_filter(
    roots: &[PathBuf],
    family_name: &str,
    column: &str,
    value: &str,
    limit: Option<usize>,
) -> da_core::Result<()> {
    let scan_result = scan_directory(roots)?;

    let family = scan_result
        .find_family(family_name)
        .ok_or_else(|| da_core::Error::FamilyNotFound(family_name.to_string()))?;

    let merged = merge_family(family)?;

    // Find the column
    let col = merged
        .find_column(column)
        .ok_or_else(|| da_core::Error::InvalidFamilyName(format!("column '{}' not found", column)))?;

    let value_lower = value.to_lowercase();

    // Filter rows where the column value contains the search string
    let matching_rows: Vec<_> = merged
        .rows
        .iter()
        .filter(|row| {
            row.cells
                .get(col.index)
                .map(|c| c.value.to_string_value().to_lowercase().contains(&value_lower))
                .unwrap_or(false)
        })
        .collect();

    if matching_rows.is_empty() {
        println!("No rows found where {} contains '{}'", column, value);
        return Ok(());
    }

    println!(
        "Found {} rows where {} contains '{}':\n",
        matching_rows.len(),
        column,
        value
    );

    // Print header
    let header: Vec<&str> = merged.columns.iter().map(|c| c.name.as_str()).collect();
    println!("{}", header.join("\t"));
    println!("{}", "-".repeat(header.len() * 12));

    // Print matching rows
    let row_limit = limit.unwrap_or(matching_rows.len());
    for row in matching_rows.iter().take(row_limit) {
        let values: Vec<String> = row
            .cells
            .iter()
            .map(|c| c.value.to_string_value())
            .collect();
        println!("{}", values.join("\t"));
    }

    if matching_rows.len() > row_limit {
        println!("... ({} more rows)", matching_rows.len() - row_limit);
    }

    Ok(())
}

fn cmd_validate(roots: &[PathBuf], patch_path: &PathBuf) -> da_core::Result<()> {
    // Load the patch file
    let patch = PatchFile::load(patch_path)?;
    println!("Validating patch for family '{}' with {} edits\n", patch.family, patch.edits.len());

    // Scan and find the family
    let scan_result = scan_directory(roots)?;
    let family = match scan_result.find_family(&patch.family) {
        Some(f) => f,
        None => {
            println!("INVALID: Family '{}' not found", patch.family);
            return Ok(());
        }
    };

    // Merge the family
    let merged = merge_family(family)?;

    // Validate each edit
    let mut valid_count = 0;
    let mut invalid_count = 0;

    for edit in &patch.edits {
        let row_exists = merged.rows.iter().any(|r| r.id == Some(edit.row_id));
        let col_exists = merged.columns.iter().any(|c| c.name == edit.column);

        if !row_exists {
            println!("INVALID: Row ID {} not found", edit.row_id);
            invalid_count += 1;
        } else if !col_exists {
            println!("INVALID: Column '{}' not found (row {})", edit.column, edit.row_id);
            invalid_count += 1;
        } else {
            // Find provenance
            if let Some(row) = merged.find_row(edit.row_id) {
                if let Some(col) = merged.find_column(&edit.column) {
                    let source = &row.cells[col.index].source;
                    let current = &row.cells[col.index].value;
                    println!(
                        "OK: Row {}, {} = '{}' -> '{}' (source: {})",
                        edit.row_id,
                        edit.column,
                        current,
                        edit.value,
                        source.file_name().unwrap_or_default().to_string_lossy()
                    );
                }
            }
            valid_count += 1;
        }
    }

    println!();
    println!("Validation complete:");
    println!("  {} valid edits", valid_count);
    println!("  {} invalid edits", invalid_count);

    if invalid_count > 0 {
        println!("\nPatch has errors and cannot be applied cleanly.");
    } else {
        println!("\nPatch is valid and ready to apply.");
    }

    Ok(())
}

fn cmd_history(history_path: &PathBuf, family: Option<&str>) -> da_core::Result<()> {
    let history = HistoryFile::load(history_path)?;

    if history.total_entries() == 0 {
        println!("No history recorded yet.");
        return Ok(());
    }

    match family {
        Some(family_name) => {
            // Show history for specific family
            match history.get_family_history(family_name) {
                Some(entries) => {
                    println!("History for '{}' ({} entries):\n", family_name, entries.len());
                    for (i, entry) in entries.iter().enumerate().rev() {
                        println!("{}. {}", i + 1, entry.timestamp.format("%Y-%m-%d %H:%M:%S"));
                        println!("   {} edits applied", entry.patch.edits.len());
                        println!("   Output: {}", entry.output_dir.display());
                        for file in &entry.output_files {
                            println!("   - {}", file.display());
                        }
                        println!();
                    }
                }
                None => {
                    println!("No history for family '{}'", family_name);
                }
            }
        }
        None => {
            // Show all history
            println!("Patch history ({} total entries):\n", history.total_entries());
            for family_name in history.families() {
                if let Some(entries) = history.get_family_history(family_name) {
                    println!("{}: {} patches applied", family_name, entries.len());
                    if let Some(last) = entries.last() {
                        println!(
                            "  Last: {} ({} edits)",
                            last.timestamp.format("%Y-%m-%d %H:%M:%S"),
                            last.patch.edits.len()
                        );
                    }
                }
            }
        }
    }

    Ok(())
}

fn cmd_undo(
    roots: &[PathBuf],
    history_path: &PathBuf,
    family_name: &str,
    output_dir: &PathBuf,
) -> da_core::Result<()> {
    let mut history = HistoryFile::load(history_path)?;

    let last_entry = match history.get_last_entry(family_name) {
        Some(entry) => entry.clone(),
        None => {
            println!("No history to undo for family '{}'", family_name);
            return Ok(());
        }
    };

    println!("Undoing last patch for '{}':", family_name);
    println!("  Applied: {}", last_entry.timestamp.format("%Y-%m-%d %H:%M:%S"));
    println!("  {} edits to undo\n", last_entry.patch.edits.len());

    // To undo, we need to re-export the original files (without the patch)
    let scan_result = scan_directory(roots)?;
    let family = scan_result
        .find_family(family_name)
        .ok_or_else(|| da_core::Error::FamilyNotFound(family_name.to_string()))?;

    let merged = merge_family(family)?;

    // Export the original (unpatched) source files
    // We need to identify which source files were modified
    let empty_patch = PatchFile::new(family_name);
    let _result = export_with_edits(&merged, &empty_patch, output_dir)?;

    // Actually, we need to re-export only the files that were modified
    // For a true undo, let's just copy the original source files
    println!("Re-exporting original files to {}:", output_dir.display());

    // Use the files that were in the last patch's output
    for output_file in &last_entry.output_files {
        if let Some(file_name) = output_file.file_name() {
            // Find the original source file
            for member in &family.members {
                if member.path.file_name() == Some(file_name) {
                    let dest = output_dir.join(file_name);
                    std::fs::copy(&member.path, &dest)?;
                    println!("  Restored: {}", dest.display());
                }
            }
        }
    }

    // Remove the entry from history
    history.pop_last_entry(family_name);
    history.save(history_path)?;

    println!("\nUndo complete. History entry removed.");

    Ok(())
}
