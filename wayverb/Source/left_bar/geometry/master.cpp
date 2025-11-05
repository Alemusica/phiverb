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

    addAndMakeVisible(report_);
    report_.setJustificationType(Justification::topLeft);

    addAndMakeVisible(sanitize_toggle_);
    addAndMakeVisible(epsilon_edit_);
    epsilon_edit_.setText("1e-6");
    epsilon_edit_.setInputRestrictions(0, "0123456789eE.-");

    addAndMakeVisible(analyze_btn_);
    analyze_btn_.addListener(this);

    addAndMakeVisible(open_logs_btn_);
    open_logs_btn_.addListener(this);

    sync_from_model();
}

void master::resized() {
    auto b = getLocalBounds().reduced(4);
    title_.setBounds(b.removeFromTop(20));
    sanitize_toggle_.setBounds(b.removeFromTop(24));
    auto row = b.removeFromTop(24);
    epsilon_edit_.setBounds(row.removeFromLeft(120));
    analyze_btn_.setBounds(row.removeFromLeft(100));
    open_logs_btn_.setBounds(row.removeFromLeft(160));
    report_.setBounds(b);
}

void master::buttonClicked(Button* b) {
    if (b == &analyze_btn_) {
        apply_to_model();
        run_analysis();
    } else if (b == &open_logs_btn_) {
        File dir{File::getSpecialLocation(File::userLibraryDirectory)
                         .getChildFile("Logs/Wayverb")};
        dir.createDirectory();
        dir.revealToUser();
    }
}

void master::sync_from_model() {
    sanitize_toggle_.setToggleState(model_.geometry.sanitize, dontSendNotification);
    epsilon_edit_.setText(String(model_.geometry.weld_epsilon));
}

void master::apply_to_model() {
    model_.geometry.sanitize = sanitize_toggle_.getToggleState();
    model_.geometry.weld_epsilon = epsilon_edit_.getText().getDoubleValue();
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

}  // namespace geometry
}  // namespace left_bar

