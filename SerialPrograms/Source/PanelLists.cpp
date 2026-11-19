/*  Left-Side Panel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QStandardItemModel>
#include <QVBoxLayout>
#include "Common/Qt/NoWheelComboBox.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "CommonFramework/Panels/UI/PanelListWidget.h"
#include "PanelCatalog.h"
#include "PanelLists.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



ProgramSelect::ProgramSelect(QWidget& parent, PanelHolder& holder)
    : QWidget(&parent)
    , m_holder(holder)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignTop);
    m_dropdown = new NoWheelCompactComboBox(this);
    m_dropdown->setMaxVisibleItems(20);
    this->setStyleSheet("QComboBox QAbstractItemView::item { min-height: 24px; }");
    {
        QListView* list_view = new QListView(this);
        list_view->setWordWrap(true);
        m_dropdown->setView(list_view);
    }
    {
//        QSize size = m_dropdown->iconSize();
//        cout << "size = " << size.width() << " x " << size.height() << endl;
//        size.setWidth(size.width() * 2);
        m_dropdown->setIconSize(QSize(48, 24));
    }
    layout->addWidget(m_dropdown);


    for (std::unique_ptr<PanelListDescriptor>& list : make_all_panel_lists()){
        add(std::move(list));
    }


    //  Load the 1st list by default.
    m_dropdown->setCurrentIndex(0);
    m_active_index = 0;
    m_active_list = m_lists[0]->make_QWidget(*this, m_holder);
    layout->addWidget(m_active_list);

    connect(
        m_dropdown, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
        this, [this](int index){
            change_list(index);
        }
    );
}

void ProgramSelect::lock(){
    m_locked = true;
    m_active_list->setEnabled(false);
}
void ProgramSelect::unlock(){
    m_locked = false;
    m_active_list->setEnabled(true);
}

void ProgramSelect::add(std::unique_ptr<PanelListDescriptor> list){
    int index = m_dropdown->count();
    const ImageViewRGB32& icon = list->icon();
    if (icon){
        m_dropdown->addItem(
            QIcon(QPixmap::fromImage(to_QImage_ref(icon))),
            QString::fromStdString(list->name())
        );
    }else{
        m_dropdown->addItem(QString::fromStdString(list->name()));
    }

    m_lists.emplace_back(std::move(list));
    const PanelListDescriptor& back = *m_lists.back();
    if (!m_tab_map.emplace(back.name(), index).second){
        const std::string name = back.name();
        m_lists.pop_back();
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Duplicate Category Name: " + name);
    }
    if (!back.enabled()){
        auto* model = qobject_cast<QStandardItemModel*>(m_dropdown->model());
        if (model != nullptr){
            QStandardItem* line_handle = model->item(index);
            if (line_handle != nullptr){
                line_handle->setEnabled(false);
            }
        }
    }
}



void ProgramSelect::load_persistent_panel(){
    const std::string* str = PERSISTENT_SETTINGS().panels.get_string("ProgramCategory");
    if (str == nullptr){
        return;
    }
    auto iter = m_tab_map.find(*str);
    if (iter == m_tab_map.end()){
        return;
    }
    m_dropdown->setCurrentIndex(iter->second);
    change_list(iter->second);
    str = PERSISTENT_SETTINGS().panels.get_string(PanelListWidget::JSON_PROGRAM_PANEL);
    if (str != nullptr){
        m_active_list->set_panel(*str);
    }
}

std::string ProgramSelect::select_program(const std::string& identifier){
    const std::optional<PanelMatch> match = find_panel(m_lists, identifier);
    if (!match){
        return "No program with identifier \"" + identifier + "\".";
    }
    if (m_locked){
        return "A program is running.";
    }
    change_list((int)match->category_index);
    m_active_list->set_panel(match->entry.display_name);
    return "";
}

void ProgramSelect::change_list(int index){
    if (m_active_index == index && m_active_list != nullptr){
        return;
    }
    m_dropdown->setCurrentIndex(index);
    m_active_index = index;
    PERSISTENT_SETTINGS().panels["ProgramCategory"] = m_lists[index]->name();
    delete m_active_list;
    m_active_list = m_lists[index]->make_QWidget(*this, m_holder);
    m_active_list->setEnabled(!m_locked);
    layout()->addWidget(m_active_list);
}

QSize ProgramSelect::sizeHint() const{
    QSize size = QWidget::sizeHint();
//    cout << size.width() << " x " << size.height() << endl;
//    cout << this->size().width() << " x " << this->size().height() << endl;
    size.setWidth(scale_dpi_width(size.width() + 10));
    return size;
}







}
