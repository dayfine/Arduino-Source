/*  Program Tabs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_ProgramTabs_H
#define PokemonAutomation_ProgramTabs_H

#include <QWidget>
#include "PanelCatalog.h"

class QComboBox;

namespace PokemonAutomation{


// The program selection UI on the left side of the program window.
// It has a dropdown menu to select which Switch game's program list to show, and
// a display list window to show the current active game's program list.
// This class owns the a vector of PanelListDescriptor. A PanelListDescriptor is
// like a generator for the programs and their UIs for a game.
class ProgramSelect : public QWidget{
public:
    ProgramSelect(QWidget& parent, PanelHolder& holder);

    void lock();
    void unlock();

    // Load the panel specified in the persistent setting.
    void load_persistent_panel();

    //  Shows the program with this identifier ("<game>:<program>"), as
    //  clicking it does. Returns an error message, or "" on success.
    std::string select_program(const std::string& identifier);

    virtual QSize sizeHint() const override;

private:
    void add_dropdown_entry(const PanelListDescriptor& list, int index);
    void change_list(int index);

private:
    PanelHolder& m_holder;

    PanelCatalog m_catalog;

    QComboBox* m_dropdown;

    bool m_locked = false;

    int m_active_index = -1;
    PanelListWidget* m_active_list = nullptr;
};



}
#endif
