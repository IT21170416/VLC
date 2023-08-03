/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2.12

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    // Properties

    property var model
    property var sortMenu

    // Settings

    pageModel: [{
        name: "base",
        component: componentBase
    }, {
        name: "group",
        component: componentGroup
    }, {
        name: "recentVideos",
        component: componentRecentVideos
    }]

    loadDefaultView: function () {
        History.update(["mc", "video", "all", "base"])
        loadPage("base")
    }

    // Events

    onCurrentItemChanged: {
        model     = currentItem.model
        sortMenu  = currentItem.sortMenu
    }
    // Functions private

    function _updateHistoryAll(index) {
        History.update(["mc", "video", "all", "base", { "initialIndex": index }])
    }

    function _updateHistoryGroup(group) {
        History.update(["mc", "video", "all", "group", {
                            "initialIndex": group.currentIndex,
                            "parentId" : group.parentId,
                            "title" : group.title
                        }])
    }

    // Children

    Component {
        id: componentBase

        VideoAllSubDisplay {
            // Events

            onShowList: History.push(["mc", "video", "all", "group",
                                      { parentId: model.id, title: model.title }], reason)

            // NOTE: The model can change over time.
            onModelChanged: root.model = model

            onCurrentIndexChanged: root._updateHistoryAll(currentIndex)
        }
    }

    Component {
        id: componentGroup

        MediaGroupDisplay {
            id: group

            onCurrentIndexChanged: root._updateHistoryGroup(group)
            onParentIdChanged    : root._updateHistoryGroup(group)
            onTitleChanged       : root._updateHistoryGroup(group)

            function isInfoExpandPanelAvailable(/* modelIndexData */) {
                return true
            }
        }
    }

    Component {
        id: componentRecentVideos

        VideoRecentVideosDisplay {
            function isInfoExpandPanelAvailable(/* modelIndexData */) {
                return true
            }
        }
    }

}
