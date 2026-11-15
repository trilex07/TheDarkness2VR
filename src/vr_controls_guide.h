#pragma once

namespace d2vr_settings {
struct GuideLine {
    const wchar_t* sense;
    const wchar_t* touch=nullptr;
    const wchar_t* Label(unsigned family) const {return family && touch ? touch : sense;}
};
inline constexpr const wchar_t* GuideFamilies[]={L"PS VR2 Sense",L"Meta Quest Touch"};
inline constexpr const wchar_t* GuideTopics[]={L"Movement",L"Weapons",L"Tentacles",L"Menus"};
inline constexpr GuideLine GuideLines[][7]={
    {
        {L"Move: left stick. Turn: right stick left / right."},
        {L"Sprint: press the left stick while moving."},
        {L"Jump: flick the right stick up."},
        {L"Crouch / stand: flick the right stick down."},
        {L"Objective: hold Circle (right hand) for 1 second.",
         L"Objective: hold B (right hand) for 1 second."},
        {L"Keep holding for guidance; follow the purple trail."},
        {L"A quick tap still uses the unlocked ability."},
    },{
        {L"Fire: R2 for the right gun, L2 for the left gun.",
         L"Fire: right trigger for the right gun, left for the left."},
        {L"Reload: tap Cross. Square is the native use button.",
         L"Reload: tap A. X is the native use button."},
        {L"Weapon wheel: hold the right stick click (R3).",
         L"Weapon wheel: press and hold the right stick."},
        {L"Move your right hand toward a weapon to highlight it."},
        {L"Release the stick click to equip the selection."},
        {L"For a two-handed gun, put your left hand by the"},
        {L"front of the gun and hold the left grip to steady it."},
    },{
        {L"Grab: aim with the left controller and squeeze L1.",
         L"Grab: aim with the left controller and squeeze left grip."},
        {L"With eye tracking enabled, look at the object to aim."},
        {L"Throw: swing your left hand while holding L1.",
         L"Throw: swing your left hand while holding left grip."},
        {L"Release the grip during the forward swing."},
        {L"Slash: hold R1 and swing sideways or up / down.",
         L"Slash: hold right grip and swing sideways or up / down."},
        {L"Hold a grabbed enemy and follow the in-game"},
        {L"prompts for the available Darkness actions."},
    },{
        {L"Select here: point with the right hand and pull R2.",
         L"Select here: point with the right hand and pull trigger."},
        {L"Back / close: Circle (right hand).",
         L"Back / close: B (right hand)."},
        {L"Pause: double-tap Triangle (left hand).",
         L"Pause: double-tap Y (left hand)."},
        {L"During story or execution choices, use the menu button."},
        {L"Recenter: click both sticks together."},
        {L"Hold both sticks clicked for 1.5 s: toggle VR / cinema."},
        {L"The manual VR / cinema override lasts 60 seconds."},
    },
};
}
