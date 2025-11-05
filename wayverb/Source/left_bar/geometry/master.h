#pragma once

#include "../master.h"
#include "../JuceLibraryCode/JuceHeader.h"

class main_model;

namespace left_bar {
namespace geometry {

class master final : public Component, public Button::Listener {
public:
    explicit master(::main_model& model);

    void resized() override;
    void buttonClicked(Button* b) override;

private:
    void run_analysis();
    void sync_from_model();
    void apply_to_model();

    ::main_model& model_;

    Label title_{}, report_{};
    ToggleButton sanitize_toggle_{"Sanitize on render (weld/remove degenerates)"};
    TextEditor epsilon_edit_{};
    TextButton analyze_btn_{"Analyze"};
    TextButton open_logs_btn_{"Open Logs Folder"};
};

}  // namespace geometry
}  // namespace left_bar

// Ensure implementation is visible to Xcode target without project edits.
// This includes the .cpp directly so that any TU including this header
// will also compile the implementation.
#include "master.cpp"
