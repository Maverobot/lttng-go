package main

import (
	"fmt"
	"io"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

type lttngDelegate struct {
	defaultDelegate list.DefaultDelegate
}

func newLttngDelegate() lttngDelegate {
	return lttngDelegate{
		defaultDelegate: list.NewDefaultDelegate(),
	}
}

// Height returns the delegate's preferred height.
func (d lttngDelegate) Height() int {
	return d.defaultDelegate.Height()
}

// Render prints an item.
func (d lttngDelegate) Render(w io.Writer, m list.Model, index int, item list.Item) {
	var (
		title, desc             string
		matchedRunesTitle       []int
		matchedRunesDescription []int
		s                       = &d.defaultDelegate.Styles
	)

	if i, ok := item.(list.DefaultItem); ok {
		title = i.Title()
		desc = i.Description()
	} else {
		return
	}

	// Conditions
	var (
		isSelected  = index == m.Index()
		emptyFilter = m.FilterState() == list.Filtering && m.FilterValue() == ""
		isFiltered  = m.FilterState() == list.Filtering || m.FilterState() == list.FilterApplied
	)

	if isFiltered {
		// Get indices of matched characters
		matchedRunes := m.MatchesForItem(index)
		matchedRunesTitle, matchedRunesDescription = getMatchedRunesForTitleAndDesciption(matchedRunes, len(title))
	}

	if emptyFilter {
		title = s.DimmedTitle.Render(title)
		desc = s.DimmedDesc.Render(desc)
	} else if isSelected && m.FilterState() != list.Filtering {
		if isFiltered {
			// Highlight matches
			{
				unmatched := s.SelectedTitle.Inline(false).UnsetPadding().UnsetBorderStyle()
				matched := unmatched.Copy().Inherit(s.FilterMatch)
				title = lipgloss.StyleRunes(title, matchedRunesTitle, matched, unmatched)
			}
			{
				unmatched := s.SelectedDesc.Inline(false).UnsetPadding().UnsetBorderStyle()
				matched := unmatched.Copy().Inherit(s.FilterMatch)
				desc = lipgloss.StyleRunes(desc, matchedRunesDescription, matched, unmatched)
			}
		}
		title = s.SelectedTitle.Render(title)
		desc = s.SelectedDesc.Render(desc)
	} else {
		if isFiltered {
			// Highlight matches
			{
				unmatched := s.NormalTitle.Inline(false).UnsetPadding()
				matched := unmatched.Copy().Inherit(s.FilterMatch)
				title = lipgloss.StyleRunes(title, matchedRunesTitle, matched, unmatched)
			}
			{
				unmatched := s.NormalDesc.Inline(false).UnsetPadding()
				matched := unmatched.Copy().Inherit(s.FilterMatch)
				desc = lipgloss.StyleRunes(desc, matchedRunesDescription, matched, unmatched)
			}
		}
		title = s.NormalTitle.Render(title)
		desc = s.NormalDesc.Render(desc)
	}

	if d.defaultDelegate.ShowDescription {
		fmt.Fprintf(w, "%s\n%s", title, desc)
		return
	}
	fmt.Fprintf(w, "%s", title)
}

// Spacing returns the delegate's spacing.
func (d lttngDelegate) Spacing() int {
	return d.defaultDelegate.Spacing()
}

// Update checks whether the delegate's UpdateFunc is set and calls it.
func (d lttngDelegate) Update(msg tea.Msg, m *list.Model) tea.Cmd {
	return d.defaultDelegate.Update(msg, m)
}

func getMatchedRunesForTitleAndDesciption(matchedRunes []int, titleLength int) ([]int, []int) {
	var matchedRunesTitle []int
	var matchedRunesDescription []int

	for _, index := range matchedRunes {
		if index < titleLength {
			matchedRunesTitle = append(matchedRunesTitle, index)
		} else {
			matchedRunesDescription = append(matchedRunesDescription, index-titleLength)
		}
	}
	return matchedRunesTitle, matchedRunesDescription
}
