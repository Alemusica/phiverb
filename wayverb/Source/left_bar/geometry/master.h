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
    void open_mesh_tools_doc();
    void reveal_mesh_folder();

    ::main_model& model_;

    Label title_{}, report_{}, instructions_{};
    TextEditor epsilon_edit_{};
    TextButton analyze_btn_{"Analyze"};
    TextButton open_logs_btn_{"Open Logs Folder"};
    TextButton mesh_tools_btn_{"Open Mesh Tools Guide"};
    TextButton mesh_folder_btn_{"Open geometrie_wayverb Folder"};
};

}  // namespace geometry
}  // namespace left_bar

// Ensure implementation is visible to Xcode target without project edits.
// This includes the .cpp directly so that any TU including this header
// will also compile the implementation.
#include "master.cpp"
