/*****************************************************************************
 * VLCLibraryWindowSidebarRootViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>

@class VLCRoundedCornerTextField;
@class VLCLibraryWindow;
@class VLCLibraryWindowChaptersSidebarViewController;
@class VLCLibraryWindowPlaylistSidebarViewController;
@class VLCLibraryWindowTitlesSidebarViewController;

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryWindow;

@interface VLCLibraryWindowSidebarRootViewController : NSViewController

@property (readonly, weak) VLCLibraryWindow *libraryWindow;

@property (readwrite, weak) IBOutlet NSSegmentedControl *viewSelector;
@property (readwrite, weak) IBOutlet NSView *targetView;

// Retain strongly as when showing/hiding the view selector this can get deallocated otherwise
@property (readwrite, strong) IBOutlet NSLayoutConstraint *topInternalConstraint;

@property (readwrite, nonatomic) BOOL mainVideoModeEnabled;

@property (readonly) VLCRoundedCornerTextField *counterLabel;
@property (readonly) NSLayoutConstraint *counterLabelInHeaderConstraint;
@property (readonly) NSLayoutConstraint *counterLabelInChildViewConstraint;
@property (readonly) NSLayoutConstraint *playlistHeaderTopConstraint;
@property (readonly) NSTextField *playlistHeaderLabel;

@property (readonly) VLCLibraryWindowPlaylistSidebarViewController *playlistSidebarViewController;
@property (readonly) VLCLibraryWindowChaptersSidebarViewController *chaptersSidebarViewController;
@property (readonly) VLCLibraryWindowTitlesSidebarViewController *titlesSidebarViewController;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;

- (IBAction)viewSelectorAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
