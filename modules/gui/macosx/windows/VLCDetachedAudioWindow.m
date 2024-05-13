/*****************************************************************************
* VLCDetachedAudioWindow.m: macOS user interface
*****************************************************************************
* Copyright (C) 2019 VLC authors and VideoLAN
*
* Author: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCDetachedAudioWindow.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCInputItem.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "views/VLCBottomBarView.h"

#import "windows/controlsbar/VLCControlsBarCommon.h"

#import "windows/video/VLCMainVideoViewOverlayView.h"

@interface VLCDetachedAudioWindow()
{
    VLCPlayerController *_playerController;
}
@end

@implementation VLCDetachedAudioWindow

- (void)awakeFromNib
{
    self.title = @"";

    _playerController = VLCMain.sharedInstance.playlistController.playerController;

    VLCTrackingView * const trackingView = self.contentView;
    trackingView.viewToHide = self.overlayView;
    trackingView.animatesTransition = YES;

    self.overlayView.drawGradientForTopControls = YES;
    self.overlayView.darkestGradientColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.8];

    self.bottomBarView.drawBorder = NO;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(inputItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];

    [self inputItemChanged:nil];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)inputItemChanged:(NSNotification *)aNotification
{
    VLCInputItem * const currentInput = _playerController.currentMedia;
    if (currentInput) {
        [self.imageView setImageURL:currentInput.artworkURL placeholderImage:[NSImage imageNamed:@"noart.png"]];
    } else {
        [self.imageView setImage:[NSImage imageNamed:@"noart.png"]];
    }
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

- (BOOL)isMovableByWindowBackground
{
    return YES;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if (event.modifierFlags & NSEventModifierFlagCommand && event.keyCode == 13) {
        [self closeAndAnimate:YES];
        return YES;
    }
    return [super performKeyEquivalent:event];
}

@end
