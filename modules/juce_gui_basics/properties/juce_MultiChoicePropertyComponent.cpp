/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/


namespace juce
{

//==============================================================================
class StringComparator
{
public:
    static int compareElements (var first, var second)
    {
        if (first.toString() > second.toString())
            return 1;
        else if (first.toString() < second.toString())
            return -1;

        return 0;
    }
};

//==============================================================================
class MultiChoicePropertyComponent::MultiChoiceRemapperSource    : public Value::ValueSource,
                                                                   private Value::Listener
{
public:
    MultiChoiceRemapperSource (const Value& source, var v)
        : sourceValue (source),
          varToControl (v)
    {
        sourceValue.addListener (this);
    }

    var getValue() const override
    {
        if (auto* arr = sourceValue.getValue().getArray())
            if (arr->contains (varToControl))
                return true;

        return false;
    }

    void setValue (const var& newValue) override
    {
        if (auto* arr = sourceValue.getValue().getArray())
        {
            auto temp = *arr;

            if (static_cast<bool> (newValue))
                temp.addIfNotAlreadyThere (varToControl);
            else
                temp.remove (arr->indexOf (varToControl));

            StringComparator c;
            temp.sort (c);

            sourceValue = temp;
        }
    }

private:
    Value sourceValue;
    var varToControl;

    void valueChanged (Value&) override    { sendChangeMessage (true); }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiChoiceRemapperSource)
};

//==============================================================================
class MultiChoicePropertyComponent::MultiChoiceRemapperSourceWithDefault    : public Value::ValueSource,
                                                                              private Value::Listener
{
public:
    MultiChoiceRemapperSourceWithDefault (ValueWithDefault& vwd, var v)
        : valueWithDefault (vwd),
          sourceValue (valueWithDefault.getPropertyAsValue()),
          varToControl (v)
    {
        sourceValue.addListener (this);
    }

    var getValue() const override
    {
        if (auto* arr = valueWithDefault.get().getArray())
            if (arr->contains (varToControl))
                return true;

        return false;
    }

    void setValue (const var& newValue) override
    {
        if (auto* arr = valueWithDefault.get().getArray())
        {
            auto temp = *arr;

            if (static_cast<bool> (newValue))
                temp.addIfNotAlreadyThere (varToControl);
            else
                temp.remove (arr->indexOf (varToControl));

            StringComparator c;
            temp.sort (c);

            valueWithDefault = temp;
        }
    }

private:
    ValueWithDefault& valueWithDefault;
    Value sourceValue;
    var varToControl;

    void valueChanged (Value&) override    { sendChangeMessage (true); }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiChoiceRemapperSourceWithDefault)
};

//==============================================================================
MultiChoicePropertyComponent::MultiChoicePropertyComponent (const String& propertyName,
                                                            const StringArray& choices,
                                                            const Array<var>& correspondingValues)
    : PropertyComponent (propertyName, 70)
{
    // The array of corresponding values must contain one value for each of the items in
    // the choices array!
    jassert (choices.size() == correspondingValues.size());

    ignoreUnused (correspondingValues);

    for (auto choice : choices)
        addAndMakeVisible (choiceButtons.add (new ToggleButton (choice)));

    maxHeight = (choiceButtons.size() * 25) + 20;

    {
        Path expandShape;
        expandShape.addTriangle ({ 0, 0 }, { 5, 10 }, { 10, 0});
        expandButton.setShape (expandShape, true, true, false);
    }

    expandButton.onClick = [this] { setExpanded (! expanded); };
    addAndMakeVisible (expandButton);

    lookAndFeelChanged();
}

MultiChoicePropertyComponent::MultiChoicePropertyComponent (const Value& valueToControl,
                                                            const String& propertyName,
                                                            const StringArray& choices,
                                                            const Array<var>& correspondingValues)
    : MultiChoicePropertyComponent (propertyName, choices, correspondingValues)
{
    // The value to control must be an array!
    jassert (valueToControl.getValue().isArray());

    for (int i = 0; i < choiceButtons.size(); ++i)
        choiceButtons[i]->getToggleStateValue().referTo (Value (new MultiChoiceRemapperSource (valueToControl,
                                                                                               correspondingValues[i])));
}

MultiChoicePropertyComponent::MultiChoicePropertyComponent (ValueWithDefault& valueToControl,
                                                            const String& propertyName,
                                                            const StringArray& choices,
                                                            const Array<var>& correspondingValues)
    : MultiChoicePropertyComponent (propertyName, choices, correspondingValues)
{
    // The value to control must be an array!
    jassert (valueToControl.get().isArray());

    for (int i = 0; i < choiceButtons.size(); ++i)
        choiceButtons[i]->getToggleStateValue().referTo (Value (new MultiChoiceRemapperSourceWithDefault (valueToControl,
                                                                                                          correspondingValues[i])));

    valueToControl.onDefaultChange = [this] { repaint(); };
}

void MultiChoicePropertyComponent::paint (Graphics& g)
{
    g.setColour (findColour (TextEditor::backgroundColourId));
    g.fillRect (getLookAndFeel().getPropertyComponentContentPosition (*this));

    if (! expanded)
    {
        g.setColour (findColour (PropertyComponent::labelTextColourId).withAlpha (0.4f));
        g.drawFittedText ("+ " + String (numHidden) + " more", getLookAndFeel().getPropertyComponentContentPosition (*this)
                                                                               .removeFromBottom (20).withTrimmedLeft (10),
                          Justification::centredLeft, 1);
    }

    PropertyComponent::paint (g);
}

void MultiChoicePropertyComponent::resized()
{
    auto bounds = getLookAndFeel().getPropertyComponentContentPosition (*this);

    bounds.removeFromBottom (5);
    expandButton.setBounds (bounds.removeFromBottom (10));

    numHidden = 0;

    for (auto* b : choiceButtons)
    {
        if (bounds.getHeight() >= 25)
        {
            b->setVisible (true);
            b->setBounds (bounds.removeFromTop (25).reduced (5, 2));
        }
        else
        {
            b->setVisible (false);
            ++numHidden;
        }
    }
}

void MultiChoicePropertyComponent::setExpanded (bool isExpanded) noexcept
{
    if (expanded == isExpanded)
        return;

    expanded = isExpanded;
    preferredHeight = expanded ? maxHeight : 70;

    if (auto* propertyPanel = findParentComponentOfClass<PropertyPanel>())
        propertyPanel->resized();

    if (onHeightChange != nullptr)
        onHeightChange();

    expandButton.setTransform (AffineTransform::rotation (expanded ? MathConstants<float>::pi : MathConstants<float>::twoPi,
                                                          (float) expandButton.getBounds().getCentreX(),
                                                          (float) expandButton.getBounds().getCentreY()));
}

//==============================================================================
void MultiChoicePropertyComponent::lookAndFeelChanged()
{
    auto iconColour = findColour (PropertyComponent::labelTextColourId);
    expandButton.setColours (iconColour, iconColour.darker(), iconColour.darker());
}

} // namespace juce