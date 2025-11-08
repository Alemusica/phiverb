#include "master.h"

#include "../../main_model.h"

#include "core/geometry_analysis.h"
#include "core/conversions.h"

namespace left_bar {
namespace geometry {

master::master(::main_model& model) : model_{model} {
    addAndMakeVisible(title_);
    title_.setText("Geometry analysis", dontSendNotification);
    title_.setJustificationType(Justification::centredLeft);

    instructions_.setText(
            "1) Preprocess OBJ via tools/wayverb_mesh.py (sanitize, triangulate).\n"
            "2) Keep cleaned meshes under geometrie_wayverb/.\n"
            "3) Use Analyze to inspect the current project scene.",
            dontSendNotification);
    instructions_.setJustificationType(Justification::topLeft);
    instructions_.setColour(Label::textColourId, Colours::lightgrey);
    addAndMakeVisible(instructions_);

    addAndMakeVisible(report_);
    report_.setJustificationType(Justification::topLeft);

    addAndMakeVisible(epsilon_edit_);
    epsilon_edit_.setText("1e-6");
    epsilon_edit_.setInputRestrictions(0, "0123456789eE.-");

    addAndMakeVisible(analyze_btn_);
    analyze_btn_.addListener(this);

    addAndMakeVisible(open_logs_btn_);
    open_logs_btn_.addListener(this);

    addAndMakeVisible(mesh_tools_btn_);
    mesh_tools_btn_.addListener(this);

    addAndMakeVisible(mesh_folder_btn_);
    mesh_folder_btn_.addListener(this);

    // Ensure the wrapper computes a non‑zero preferred height.
    // PropertyPanel queries content.getHeight() when wrapped, so we must
    // provide an initial size here; layout adapts in resized().
    setSize(300, 260);
}

void master::resized() {
    auto b = getLocalBounds().reduced(4);
    title_.setBounds(b.removeFromTop(20));
    instructions_.setBounds(b.removeFromTop(72));
    auto row = b.removeFromTop(24);
    epsilon_edit_.setBounds(row.removeFromLeft(120));
    analyze_btn_.setBounds(row.removeFromLeft(100));
    open_logs_btn_.setBounds(row.removeFromLeft(160));
    auto row2 = b.removeFromTop(28);
    mesh_tools_btn_.setBounds(row2.removeFromLeft(220));
    mesh_folder_btn_.setBounds(row2);
    report_.setBounds(b);
}

void master::buttonClicked(Button* b) {
    if (b == &analyze_btn_) {
        run_analysis();
    } else if (b == &open_logs_btn_) {
        // Use home directory and append Library/Logs/Wayverb for portability
        File dir{File::getSpecialLocation(File::userHomeDirectory)
                         .getChildFile("Library/Logs/Wayverb")};
        dir.createDirectory();
        dir.revealToUser();
    } else if (b == &mesh_tools_btn_) {
        open_mesh_tools_doc();
    } else if (b == &mesh_folder_btn_) {
        reveal_mesh_folder();
    }
}

void master::run_analysis() {
    const auto scene = model_.project.get_scene_data();
    const auto rep = wayverb::core::analyze_geometry(scene, static_cast<float>(epsilon_edit_.getText().getDoubleValue()));
    String text;
    text << "vertices: " << (int64)rep.vertices << "\n";
    text << "triangles: " << (int64)rep.triangles << "\n";
    text << "zero-area: " << (int64)rep.zero_area << "\n";
    text << "duplicate vertices: " << (int64)rep.duplicate_vertices << "\n";
    text << "boundary edges: " << (int64)rep.boundary_edges << "\n";
    text << "non-manifold edges: " << (int64)rep.non_manifold_edges << "\n";
    text << "watertight: " << (rep.watertight ? "yes" : "no") << "\n";
    report_.setText(text, dontSendNotification);
}

static File locate_repo_root() {
    auto path = File::getSpecialLocation(File::currentApplicationFile);
    // move from .../wayverb.app/Contents/MacOS/wayverb up to repo root
    for (int i = 0; i < 12; ++i) {
        if (path.getChildFile("docs/mesh_tools.md").existsAsFile() ||
            path.getChildFile(".git").isDirectory()) {
            return path;
        }
        const auto parent = path.getParentDirectory();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return {};
}

void master::open_mesh_tools_doc() {
    if (auto root = locate_repo_root(); root.exists()) {
        auto doc = root.getChildFile("docs/mesh_tools.md");
        if (doc.existsAsFile()) {
            doc.startAsProcess();
            return;
        }
    }
    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                                     "Mesh tools",
                                     "Could not locate docs/mesh_tools.md relative to this build. "
                                     "Please open the repository and view docs/mesh_tools.md manually.");
}

void master::reveal_mesh_folder() {
    if (auto root = locate_repo_root(); root.exists()) {
        auto folder = root.getChildFile("geometrie_wayverb");
        folder.createDirectory();
        folder.revealToUser();
        return;
    }
    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                                     "Mesh folder",
                                     "Could not locate geometrie_wayverb/. "
                                     "If you are running outside the source tree, "
                                     "open the folder manually.");
}

}  // namespace geometry
}  // namespace left_bar
