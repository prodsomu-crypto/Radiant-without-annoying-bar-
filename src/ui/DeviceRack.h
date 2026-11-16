#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "core/Effects.h"
#include "engine/AudioEngine.h"
#include "ui/ControllerEditors.h"
#include "ui/Controllers164.h"
#include "ui/FacetEqEditor.h"
#include "ui/FlExtraEditors.h"
#include "ui/HintBar.h"
#include "ui/I18n.h"
#include "ui/NewtoneEditor.h"
#include "ui/ParametricEqEditor.h"
#include "ui/PatcherEditor.h"
#include "ui/PluginEditors.h"
#include "ui/ProEditors.h"
#include "ui/QuickPick.h"
#include "ui/Theme.h"
#include "ui/VisualEditors.h"

namespace lynx
{
class DeviceRackComponent final : public juce::Component,
                                  private juce::ChangeListener
{
public:
    std::function<void()> onClose;
    std::function<void (int)> onResizeHeight;

    explicit DeviceRackComponent (AudioEngine& e) : engine (e)
    {
        engine.addChangeListener (this);

        trackViewport.setViewedComponent (&trackList, false);
        trackViewport.setScrollBarsShown (true, false);
        trackViewport.setScrollBarThickness (8);
        addAndMakeVisible (trackViewport);

        chainViewport.setViewedComponent (&chainContent, false);
        chainViewport.setScrollBarsShown (false, true);
        chainViewport.setScrollBarThickness (12);
        chainViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
        addAndMakeVisible (chainViewport);

        scrollLeftBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
        scrollRightBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
        closeBtn.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        for (auto* b : { &scrollLeftBtn, &scrollRightBtn, &closeBtn })
        {
            b->setWantsKeyboardFocus (false);
            b->setMouseClickGrabsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, theme::colour::bg2);
            b->setColour (juce::TextButton::textColourOffId, theme::colour::textPrimary);
            addAndMakeVisible (*b);
        }
        scrollLeftBtn.onClick = [this] { chainViewport.setViewPosition (juce::jmax (0, chainViewport.getViewPositionX() - 160), 0); };
        scrollRightBtn.onClick = [this] { chainViewport.setViewPosition (chainViewport.getViewPositionX() + 160, 0); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
    }

    ~DeviceRackComponent() override { engine.removeChangeListener (this); }

    void visibilityChanged() override
    {
        if (isVisible())
            rebuildAll (true);
        else
            clearCards();
    }

    void setSelectedTrack (int t)
    {
        const auto count = (int) engine.getSong().mixer.size();
        t = juce::jlimit (0, juce::jmax (0, count - 1), t);
        if (selectedTrack != t)
        {
            selectedTrack = t;
            rebuildAll (true);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::colour::bg0);
        auto top = getLocalBounds().removeFromTop (topBarHeight);
        g.setColour (theme::colour::bg2);
        g.fillRect (top);
        g.setColour (theme::colour::borderStrong);
        g.fillRect (0, 0, getWidth(), 2);
        g.drawHorizontalLine (topBarHeight - 1, 0.0f, (float) getWidth());
        g.drawVerticalLine (trackListWidth, (float) topBarHeight, (float) getHeight());

        g.setColour (theme::colour::textDisabled);
        g.fillRoundedRectangle ((float) getWidth() * 0.5f - 18.0f, 3.0f, 36.0f, 3.0f, 1.5f);

        const auto& song = engine.getSong();
        juce::String title = tr ("Device Rack");
        if (selectedTrack >= 0 && selectedTrack < (int) song.mixer.size())
        {
            const auto& mt = song.mixer[(std::size_t) selectedTrack];
            const auto tName = mt.name.empty()
                ? (selectedTrack == 0 ? tr ("Master") : tr ("Insert {0}", { juce::String (selectedTrack) }))
                : juce::String::fromUTF8 (mt.name.c_str());
            title << "  -  " << tName;
        }
        g.setColour (theme::colour::textPrimary);
        g.setFont (theme::font (theme::type::small, true));
        g.drawText (title, 10, 0, juce::jmax (40, getWidth() - 110), topBarHeight, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto top = area.removeFromTop (topBarHeight).reduced (4, 3);
        closeBtn.setBounds (top.removeFromRight (24));
        top.removeFromRight (6);
        scrollRightBtn.setBounds (top.removeFromRight (24));
        top.removeFromRight (2);
        scrollLeftBtn.setBounds (top.removeFromRight (24));

        trackViewport.setBounds (area.removeFromLeft (trackListWidth));
        area.removeFromLeft (1);
        chainViewport.setBounds (area);
        layoutTrackList();
        layoutChain();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < topBarHeight)
        {
            resizingHeight = true;
            dragStartH = getHeight();
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (resizingHeight && onResizeHeight)
            onResizeHeight (dragStartH - e.getDistanceFromDragStartY());
    }
    void mouseUp (const juce::MouseEvent&) override { resizingHeight = false; }

private:
    static constexpr int topBarHeight = 26, trackListWidth = 142;

    struct TrackRow final : public juce::Component
    {
        std::function<void()> onTap;
        juce::String label;
        int fxCount = 0;
        bool active = false;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (3, 1).toFloat();
            if (active)
            {
                g.setColour (theme::colour::accent.withAlpha (0.22f));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (theme::colour::accent);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
            }
            g.setColour (active ? theme::colour::textPrimary : theme::colour::textSecondary);
            g.setFont (theme::font (theme::type::small, active));
            auto textR = getLocalBounds().reduced (8, 0);
            if (fxCount > 0)
            {
                auto badge = textR.removeFromRight (20).withSizeKeepingCentre (18, 15);
                g.setColour (active ? theme::colour::accent : theme::colour::bg4);
                g.fillRoundedRectangle (badge.toFloat(), 4.0f);
                g.setColour (active ? theme::colour::onAccent : theme::colour::textPrimary);
                g.setFont (theme::font (9.5f, true));
                g.drawText (juce::String (fxCount), badge, juce::Justification::centred, false);
                g.setColour (active ? theme::colour::textPrimary : theme::colour::textSecondary);
                g.setFont (theme::font (theme::type::small, active));
            }
            g.drawFittedText (label, textR, juce::Justification::centredLeft, 1, 0.8f);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mouseWasClicked() && onTap) onTap();
        }
    };

    struct CardHolder final : public juce::Component
    {
        int track = 0, slot = 0;
        EffectKind kind = EffectKind::eq;
        int natW = 280, natH = 180;
        juce::Component bodyClip;
        std::unique_ptr<juce::Component> editor;
        juce::TextButton bypassBtn, popBtn, removeBtn;
        juce::Slider mixSlider;
        juce::Label fallbackLabel;
        juce::String titleText;
        std::function<void()> onBypass, onPop, onRemove;
        std::function<void (float)> onMix;

        CardHolder (AudioEngine& eng, int t, int s, const EffectSlot& fx)
            : track (t), slot (s), kind (fx.kind)
        {
            const auto& info = effectInfo (fx.kind);
            titleText = juce::String (slot + 1) + ". " + juce::String::fromUTF8 (info.name);

            bypassBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f"));
            bypassBtn.setClickingTogglesState (true);
            bypassBtn.setToggleState (fx.enabled, juce::dontSendNotification);
            bypassBtn.setColour (juce::TextButton::buttonOnColourId, theme::colour::accent.withAlpha (0.28f));
            bypassBtn.setColour (juce::TextButton::textColourOnId, theme::colour::accent);
            bypassBtn.setColour (juce::TextButton::textColourOffId, theme::colour::textDisabled);
            bypassBtn.onClick = [this] { if (onBypass) onBypass(); };

            popBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x97"));
            popBtn.setTooltip (tr ("Open full window"));
            popBtn.onClick = [this] { if (onPop) onPop(); };

            removeBtn.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
            removeBtn.setTooltip (tr ("Remove effect"));
            removeBtn.onClick = [this] { if (onRemove) onRemove(); };

            for (auto* b : { &bypassBtn, &popBtn, &removeBtn })
            {
                b->setWantsKeyboardFocus (false);
                b->setMouseClickGrabsKeyboardFocus (false);
                addAndMakeVisible (*b);
            }

            addAndMakeVisible (bodyClip);
            editor = createEmbedded (eng, track, slot, fx);
            if (editor != nullptr)
            {
                natW = juce::jmax (260, editor->getWidth());
                natH = juce::jmax (160, editor->getHeight());
                editor->setBounds (0, 0, natW, natH);
                bodyClip.addAndMakeVisible (*editor);
            }
            else
            {
                natW = 180; natH = 170;
                fallbackLabel.setText (juce::String::fromUTF8 (info.name) + "\n" + tr ("(Tap \xe2\x86\x97 for full UI)"), juce::dontSendNotification);
                fallbackLabel.setJustificationType (juce::Justification::centred);
                fallbackLabel.setColour (juce::Label::textColourId, theme::colour::textSecondary);
                bodyClip.addAndMakeVisible (fallbackLabel);
                mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
                mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
                mixSlider.setRange (0.0, 100.0, 1.0);
                mixSlider.setTextValueSuffix ("%");
                mixSlider.setValue (fx.mix * 100.0, juce::dontSendNotification);
                mixSlider.onValueChange = [this] { if (onMix) onMix ((float) mixSlider.getValue() / 100.0f); };
                bodyClip.addAndMakeVisible (mixSlider);
            }
        }

        static std::unique_ptr<juce::Component> createEmbedded (AudioEngine& eng, int t, int s, const EffectSlot& fx)
        {
            if (fx.kind == EffectKind::patcher)
                return PatcherWindows::makeSlotEditor (eng, t, s);
            if (fx.kind == EffectKind::parametricEq || fx.kind == EffectKind::eq)
                return makeParametricEqEditor (eng, t, s);
            if (fx.kind == EffectKind::facetEq)
                return makeFacetEqEditor (eng, t, s, false);
            if (ProEditorWindows::handles (fx.kind))
                return makeProEditor (eng, t, s, false);
            if (VisualWindows::handles (fx.kind))
                return makeVisualEditor (eng, t, s, false);
            if (FlExtraWindows::handles (fx.kind))
                return makeFlExtraSlotEditor (eng, t, s, false);
            if (NewtoneWindows::handles (fx.kind))
                return makeNewtoneEditor (eng, t, s);
            if (fx.kind == EffectKind::formulaController)
                return FormulaControllerWindows::makeEditor (eng, t, s);
            if (NativeEffectWindows::canOpen (fx))
                return makeNativeEffectEditor (eng, t, s, 1.0f, 0);
            return nullptr;
        }

        int preferredWidth (int totalH) const
        {
            const int bodyH = juce::jmax (70, totalH - 26);
            if (editor == nullptr)
                return 168;
            const float sc = (float) bodyH / (float) juce::jmax (1, natH);
            return juce::jmax (130, juce::roundToInt ((float) natW * sc));
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (theme::colour::bg1);
            g.fillRoundedRectangle (r, 6.0f);
            auto hdr = getLocalBounds().removeFromTop (24);
            g.setColour (theme::colour::bg3);
            g.fillRoundedRectangle (hdr.toFloat(), 5.0f);
            g.setColour (theme::colour::border);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);

            g.setColour (bypassBtn.getToggleState() ? theme::colour::textPrimary : theme::colour::textDisabled);
            g.setFont (theme::font (11.0f, true));
            g.drawFittedText (titleText, hdr.withTrimmedLeft (26).withTrimmedRight (50), juce::Justification::centredLeft, 1, 0.75f);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto hdr = r.removeFromTop (24).reduced (3, 2);
            bypassBtn.setBounds (hdr.removeFromLeft (20));
            removeBtn.setBounds (hdr.removeFromRight (20));
            hdr.removeFromRight (2);
            popBtn.setBounds (hdr.removeFromRight (24));

            auto body = r.reduced (2, 2);
            bodyClip.setBounds (body);
            if (editor != nullptr)
            {
                const float sc = (float) body.getHeight() / (float) juce::jmax (1, natH);
                editor->setTransform (juce::AffineTransform::scale (sc));
            }
            else
            {
                fallbackLabel.setBounds (body.removeFromTop (juce::jmax (40, body.getHeight() - 34)).reduced (4));
                mixSlider.setBounds (body.reduced (8, 4));
            }
        }
    };

    static void openPluginPopup (AudioEngine& eng, int t, int s, const EffectSlot& fx)
    {
        if (fx.kind == EffectKind::parametricEq || fx.kind == EffectKind::eq) ParametricEqWindows::open (eng, t, s);
        else if (fx.kind == EffectKind::facetEq) FacetEqWindows::open (eng, t, s);
        else if (fx.kind == EffectKind::patcher) PatcherWindows::openSlot (eng, t, s);
        else if (ProEditorWindows::handles (fx.kind)) ProEditorWindows::open (eng, t, s);
        else if (VisualWindows::handles (fx.kind)) VisualWindows::open (eng, t, s);
        else if (FlExtraWindows::handles (fx.kind)) FlExtraWindows::open (eng, t, s);
        else if (NewtoneWindows::handles (fx.kind)) NewtoneWindows::open (eng, t, s);
        else if (Controller164Windows::handles (fx.kind)) Controller164Windows::open (eng, t, s);
        else if (fx.kind == EffectKind::formulaController) FormulaControllerWindows::open (eng, t, s);
        else if (NativeEffectWindows::canOpen (fx)) NativeEffectWindows::open (eng, t, s);
        else ui::postHint ({ tr ("Open external plugin from the Mixer panel") }, 1600);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (isVisible())
            rebuildAll (false);
    }

    void clearCards()
    {
        cards.clear();
        chainContent.removeAllChildren();
    }

    void rebuildAll (bool force)
    {
        const auto& song = engine.getSong();
        if (selectedTrack < 0 || selectedTrack >= (int) song.mixer.size())
            selectedTrack = 0;

        trackRows.clear();
        trackList.removeAllChildren();
        for (int i = 0; i < (int) song.mixer.size(); ++i)
        {
            const auto& mt = song.mixer[(std::size_t) i];
            auto row = std::make_unique<TrackRow>();
            row->label = mt.name.empty() ? (i == 0 ? tr ("Master") : tr ("Insert {0}", { juce::String (i) }))
                                         : juce::String::fromUTF8 (mt.name.c_str());
            row->fxCount = (int) mt.effects.size();
            row->active = (i == selectedTrack);
            row->onTap = [this, i] { setSelectedTrack (i); };
            trackList.addAndMakeVisible (*row);
            trackRows.push_back (std::move (row));
        }
        layoutTrackList();

        if (selectedTrack >= (int) song.mixer.size())
            return;
        const auto& effects = song.mixer[(std::size_t) selectedTrack].effects;

        if (! force && effects.size() == cards.size())
        {
            bool same = true;
            for (std::size_t i = 0; i < effects.size(); ++i)
                if (cards[i]->kind != effects[i].kind)
                    same = false;
            if (same)
            {
                for (std::size_t i = 0; i < cards.size(); ++i)
                {
                    cards[i]->bypassBtn.setToggleState (effects[i].enabled, juce::dontSendNotification);
                    cards[i]->mixSlider.setValue (effects[i].mix * 100.0, juce::dontSendNotification);
                    cards[i]->repaint();
                }
                repaint();
                return;
            }
        }

        clearCards();
        for (int s = 0; s < (int) effects.size(); ++s)
        {
            auto card = std::make_unique<CardHolder> (engine, selectedTrack, s, effects[(std::size_t) s]);
            int t = selectedTrack, sl = s;
            card->onBypass = [this, t, sl] {
                engine.editSong ([t, sl] (Song& sng) {
                    if (t < (int) sng.mixer.size() && sl < (int) sng.mixer[(std::size_t) t].effects.size())
                        sng.mixer[(std::size_t) t].effects[(std::size_t) sl].enabled = ! sng.mixer[(std::size_t) t].effects[(std::size_t) sl].enabled;
                });
            };
            card->onPop = [this, t, sl] {
                const auto& sg = engine.getSong();
                if (t < (int) sg.mixer.size() && sl < (int) sg.mixer[(std::size_t) t].effects.size())
                    openPluginPopup (engine, t, sl, sg.mixer[(std::size_t) t].effects[(std::size_t) sl]);
            };
            card->onRemove = [this, t, sl] {
                engine.editSong ([t, sl] (Song& sng) {
                    if (t < (int) sng.mixer.size() && sl < (int) sng.mixer[(std::size_t) t].effects.size())
                        sng.mixer[(std::size_t) t].effects.erase (sng.mixer[(std::size_t) t].effects.begin() + sl);
                });
            };
            card->onMix = [this, t, sl] (float m) {
                engine.editLevels ([t, sl, m] (Song& sng) {
                    if (t < (int) sng.mixer.size() && sl < (int) sng.mixer[(std::size_t) t].effects.size())
                        sng.mixer[(std::size_t) t].effects[(std::size_t) sl].mix = m;
                });
            };
            chainContent.addAndMakeVisible (*card);
            cards.push_back (std::move (card));
        }

        addBtn.setButtonText ("+ " + tr ("Add"));
        addBtn.setWantsKeyboardFocus (false);
        addBtn.onClick = [this] { showAddMenu(); };
        chainContent.addAndMakeVisible (addBtn);
        layoutChain();
        repaint();
    }

    void showAddMenu()
    {
        const auto& song = engine.getSong();
        if (selectedTrack >= (int) song.mixer.size() || (int) song.mixer[(std::size_t) selectedTrack].effects.size() >= 10)
        {
            ui::postHint ({ tr ("All 10 effect slots on this track are full") }, 1600);
            return;
        }
        juce::PopupMenu menu;
        quickpick::addFolderGroups (menu, engine, std::function<void (EffectSlot)> ([this, t = selectedTrack] (EffectSlot fx) {
            engine.editSong ([t, fx] (Song& s) {
                if (t < (int) s.mixer.size() && (int) s.mixer[(std::size_t) t].effects.size() < 10)
                    s.mixer[(std::size_t) t].effects.push_back (fx);
            });
        }));
        quickpick::addFolderViewItems (menu, engine);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn));
    }

    void layoutTrackList()
    {
        const int rowH = 26;
        const int w = juce::jmax (40, trackViewport.getWidth() - 8);
        trackList.setSize (w, juce::jmax (trackViewport.getHeight(), (int) trackRows.size() * rowH));
        for (int i = 0; i < (int) trackRows.size(); ++i)
            trackRows[(std::size_t) i]->setBounds (0, i * rowH, w, rowH);
    }

    void layoutChain()
    {
        const int bodyTotalH = juce::jmax (80, chainViewport.getHeight() - 14);
        int x = 8;
        for (auto& c : cards)
        {
            const int cw = c->preferredWidth (bodyTotalH);
            c->setBounds (x, 2, cw, bodyTotalH);
            x += cw + 8;
        }
        addBtn.setBounds (x, 2, 72, bodyTotalH);
        x += 80;
        chainContent.setSize (juce::jmax (chainViewport.getWidth(), x), juce::jmax (1, chainViewport.getHeight() - 12));
    }

    AudioEngine& engine;
    int selectedTrack = 0;
    bool resizingHeight = false;
    int dragStartH = 210;
    juce::Viewport trackViewport, chainViewport;
    juce::Component trackList, chainContent;
    std::vector<std::unique_ptr<TrackRow>> trackRows;
    std::vector<std::unique_ptr<CardHolder>> cards;
    juce::TextButton scrollLeftBtn, scrollRightBtn, closeBtn, addBtn;
};
} // namespace lynx
