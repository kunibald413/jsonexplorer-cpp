#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <json.hpp>
#include "imgui.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string value_to_string(const json& val) {
    if (val.is_string()) return val.get<std::string>();
    if (val.is_number_integer()) return std::to_string(val.get<int>());
    if (val.is_number_float()) return std::to_string(val.get<double>());
    if (val.is_boolean()) return val.get<bool>() ? "true" : "false";
    if (val.is_null()) return "null";
    return val.dump();
}

struct JsonTable {
    std::string filepath;                         // full path
    std::string name;
    std::vector<std::string> columns;
    std::vector<json> rows;
    int current_page = 0;
    int page_size = 100;
    int sort_column = -1;
    ImGuiSortDirection sort_direction = ImGuiSortDirection_None;
    std::string filter_string;
    std::vector<char> column_visibility;
    std::vector<size_t> filtered_row_indices;

    void rebuild_filter() {
        filtered_row_indices.clear();
        if (filter_string.empty()) {
            for (size_t i = 0; i < rows.size(); ++i)
                filtered_row_indices.push_back(i);
        } else {
            std::string lower_filter = filter_string;
            std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
            for (size_t i = 0; i < rows.size(); ++i) {
                bool matches = false;
                for (const auto& col : columns) {
                    if (rows[i].contains(col)) {
                        std::string val = value_to_string(rows[i][col]);
                        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                        if (val.find(lower_filter) != std::string::npos) {
                            matches = true;
                            break;
                        }
                    }
                }
                if (matches)
                    filtered_row_indices.push_back(i);
            }
        }
        current_page = 0;
    }

    void sort_data() {
        if (sort_column >= 0 && sort_column < (int)columns.size()) {
            std::string col = columns[sort_column];
            std::sort(filtered_row_indices.begin(), filtered_row_indices.end(),
                [this, &col](size_t a, size_t b) {
                    auto& row_a = rows[a];
                    auto& row_b = rows[b];
                    auto val_a = row_a.contains(col) ? row_a[col] : json();
                    auto val_b = row_b.contains(col) ? row_b[col] : json();
                    if (sort_direction == ImGuiSortDirection_Ascending)
                        return value_to_string(val_a) < value_to_string(val_b);
                    else
                        return value_to_string(val_a) > value_to_string(val_b);
                });
        }
    }

    bool load_from_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Cannot open " << path << std::endl;
            return false;
        }
        json j;
        try {
            file >> j;
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error in " << path << ": " << e.what() << std::endl;
            return false;
        }

        filepath = path;
        if (j.contains("table")) name = j["table"];
        else if (j.contains("name")) name = j["name"];
        else name = fs::path(path).stem().string();

        columns.clear();
        rows.clear();

        if (j.contains("columns") && j["columns"].is_array()) {
            for (const auto& col : j["columns"]) {
                if (col.contains("name"))
                    columns.push_back(col["name"]);
            }
        }
        if (columns.empty() && j.contains("rows") && !j["rows"].empty() && j["rows"][0].is_object()) {
            for (auto& [key, val] : j["rows"][0].items())
                columns.push_back(key);
        }
        if (columns.empty()) return false;

        if (j.contains("rows") && j["rows"].is_array()) {
            for (const auto& row : j["rows"])
                rows.push_back(row);
        }
        if (rows.empty()) return false;

        column_visibility.assign(columns.size(), 1);
        rebuild_filter();
        return true;
    }
};


struct ExplorerState {
    std::string current_folder;
    std::vector<std::string> json_files;            // filenames (relative to folder)
    std::unordered_map<std::string, JsonTable> loaded_tables; // key = filename
    std::string selected_filename;                   // currently selected (may not be loaded)
    char filter_buffer[256] = "";
    bool show_explorer = true;
    std::string default_table_from_config;
};

static ExplorerState g_state;

static void LoadConfig(const std::string& folder) {
    fs::path config_path = fs::path(folder) / "explorer_config.json";
    if (!fs::exists(config_path)) return;
    std::ifstream file(config_path);
    if (!file.is_open()) return;
    try {
        json config;
        file >> config;
        if (config.contains("default_table") && config["default_table"].is_string()) {
            g_state.default_table_from_config = config["default_table"];
            std::cout << "Config: default table = " << g_state.default_table_from_config << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading config: " << e.what() << std::endl;
    }
}

static void ScanJsonFiles(const std::string& folder) {
    g_state.json_files.clear();
    g_state.current_folder = folder;
    if (!fs::exists(folder) || !fs::is_directory(folder)) {
        std::cerr << "Error: " << folder << " is not a valid directory.\n";
        return;
    }
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            // ignore the config file itself
            if (entry.path().filename() == "explorer_config.json")
                continue;
            g_state.json_files.push_back(entry.path().filename().string());
        }
    }
    std::sort(g_state.json_files.begin(), g_state.json_files.end());
}

static bool EnsureTableLoaded(const std::string& filename) {
    if (g_state.loaded_tables.find(filename) != g_state.loaded_tables.end())
        return true; // already loaded

    fs::path fullpath = fs::path(g_state.current_folder) / filename;
    JsonTable table;
    if (!table.load_from_file(fullpath.string())) {
        std::cerr << "Failed to load " << fullpath << std::endl;
        return false;
    }
    g_state.loaded_tables[filename] = std::move(table);
    return true;
}

static JsonTable& GetLoadedTable(const std::string& filename) {
    return g_state.loaded_tables.at(filename);
}

// main ui
void ShowJsonExplorer(const std::string& json_folder = ".") {
    static bool initialized = false;
    if (!initialized) {
        ScanJsonFiles(json_folder);
        LoadConfig(json_folder);
        // If config has a default table and it exists in the file list, select it
        if (!g_state.default_table_from_config.empty()) {
            auto it = std::find(g_state.json_files.begin(), g_state.json_files.end(),
                                g_state.default_table_from_config);
            if (it != g_state.json_files.end()) {
                g_state.selected_filename = *it;
                // Optionally pre‑load it now (or load when first displayed)
                EnsureTableLoaded(g_state.selected_filename);
            } else {
                std::cerr << "Config default table not found: " << g_state.default_table_from_config << std::endl;
                // fallback to first file if any
                if (!g_state.json_files.empty())
                    g_state.selected_filename = g_state.json_files[0];
            }
        } else if (!g_state.json_files.empty()) {
            g_state.selected_filename = g_state.json_files[0];
        }
        initialized = true;
    }

    if (g_state.json_files.empty()) {
        ImGui::Begin("JSON Table Explorer");
        ImGui::Text("No JSON table files found in: %s", json_folder.c_str());
        ImGui::Text("(Place .json files there, optionally with explorer_config.json)");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1200, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("JSON Table Explorer", &g_state.show_explorer);

    ImGui::BeginChild("LeftPane", ImVec2(200, 0), true);
    ImGui::Text("Tables (%zu)", g_state.json_files.size());
    ImGui::Separator();
    for (const auto& fname : g_state.json_files) {
        bool is_selected = (g_state.selected_filename == fname);
        if (ImGui::Selectable(fname.c_str(), is_selected)) {
            g_state.selected_filename = fname;
            EnsureTableLoaded(fname);
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
    if (g_state.selected_filename.empty()) {
        ImGui::Text("No table selected.");
    } else {
        // Make sure it's loaded (should be by now)
        if (!EnsureTableLoaded(g_state.selected_filename)) {
            ImGui::Text("Failed to load %s", g_state.selected_filename.c_str());
        } else {
            JsonTable& table = GetLoadedTable(g_state.selected_filename);
            // Same UI as before
            ImGui::Text("Table: %s", table.name.c_str());
            ImGui::Text("Total rows: %zu | Filtered rows: %zu", table.rows.size(), table.filtered_row_indices.size());
            ImGui::Separator();

            // Filter input
            ImGui::SetNextItemWidth(250);
            if (ImGui::InputText("Filter", g_state.filter_buffer, sizeof(g_state.filter_buffer))) {
                table.filter_string = g_state.filter_buffer;
                std::transform(table.filter_string.begin(), table.filter_string.end(), table.filter_string.begin(), ::tolower);
                table.rebuild_filter();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                g_state.filter_buffer[0] = '\0';
                table.filter_string.clear();
                table.rebuild_filter();
            }

            ImGui::SameLine();
            if (ImGui::Button("Select Columns"))
                ImGui::OpenPopup("column_selector");
            if (ImGui::BeginPopup("column_selector")) {
                for (int i = 0; i < (int)table.columns.size(); i++) {
                    bool visible = table.column_visibility[i] != 0;
                    if (ImGui::Checkbox(table.columns[i].c_str(), &visible)) {
                        table.column_visibility[i] = visible ? 1 : 0;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();

            // Pagination
            int total_rows = (int)table.filtered_row_indices.size();
            int total_pages = (total_rows + table.page_size - 1) / table.page_size;
            if (total_pages == 0) total_pages = 1;

            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("Page Size", &table.page_size, 10, 100)) {
                if (table.page_size < 1) table.page_size = 1;
                table.current_page = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("<<")) table.current_page = 0;
            ImGui::SameLine();
            if (ImGui::Button("< Prev") && table.current_page > 0) table.current_page--;
            ImGui::SameLine();
            ImGui::Text("Page %d of %d", table.current_page + 1, total_pages);
            ImGui::SameLine();
            if (ImGui::Button("Next >") && table.current_page + 1 < total_pages) table.current_page++;
            ImGui::SameLine();
            if (ImGui::Button(">>")) table.current_page = total_pages - 1;

            // Visible columns
            std::vector<std::string> visible_columns;
            std::vector<int> visible_indices;
            for (int i = 0; i < (int)table.columns.size(); i++) {
                if (table.column_visibility[i]) {
                    visible_columns.push_back(table.columns[i]);
                    visible_indices.push_back(i);
                }
            }
            if (visible_columns.empty()) {
                ImGui::Text("No columns selected. Use 'Select Columns' to show data.");
            } else {
                if (ImGui::BeginTable("##table", (int)visible_columns.size(),
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                        ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {

                    for (int i = 0; i < (int)visible_columns.size(); i++) {
                        ImGui::TableSetupColumn(visible_columns[i].c_str(), ImGuiTableColumnFlags_DefaultSort, 0.0f, visible_indices[i]);
                    }
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs()) {
                        if (sorts_specs->SpecsDirty) {
                            if (sorts_specs->SpecsCount > 0) {
                                const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[0];
                                table.sort_column = (int)sort_spec->ColumnUserID;
                                table.sort_direction = sort_spec->SortDirection;
                                table.sort_data();
                            } else {
                                table.sort_column = -1;
                            }
                            sorts_specs->SpecsDirty = false;
                        }
                    }

                    ImGuiListClipper clipper;
                    int start_idx = table.current_page * table.page_size;
                    int rows_to_show = std::min(table.page_size, total_rows - start_idx);
                    clipper.Begin(rows_to_show);
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            ImGui::TableNextRow();
                            int global_row = start_idx + row;
                            if (global_row >= total_rows) break;
                            size_t original_index = table.filtered_row_indices[global_row];
                            const json& row_data = table.rows[original_index];

                            for (int col = 0; col < (int)visible_columns.size(); col++) {
                                ImGui::TableSetColumnIndex(col);
                                std::string col_name = visible_columns[col];
                                std::string display = "null";
                                if (row_data.contains(col_name))
                                    display = value_to_string(row_data[col_name]);
                                ImGui::Text("%s", display.c_str());
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}