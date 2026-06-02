# Mobile UI Widgets (Ionic 8) — UX vocabulary

A shared vocabulary of the **enhanced widgets** available in the mobile portal
(React 18 / Ionic 8). Use these names in design discussions so we mean the same
thing. KISS first: prefer the simplest widget that does the job, lean on native
transitions + back buttons, and reserve overlays for transient feedback.

Legend: ✅ in use · ◻ available · ⏳ planned.

## Transient overlays (feedback, dialogs)

| Widget            | Ionic component     | Use for                                            | Status |
| ----------------- | ------------------- | -------------------------------------------------- | ------ |
| **Toast**         | `IonToast`          | Brief, auto-dismissing messages (2–3s); can carry a button/handler to deep-link (e.g. tap an incoming-message toast → open the channel). | ⏳ |
| **Alert**         | `IonAlert`          | Confirm/destructive prompts, simple inputs.        | ◻ |
| **Action Sheet**  | `IonActionSheet`    | A short menu of actions slid up from the bottom.   | ◻ |
| **Modal**         | `IonModal`          | Full-screen or **bottom-sheet** task flows (e.g. attach/upload, compose). Supports `breakpoints` for sheet behavior. | ◻ |
| **Popover**       | `IonPopover`        | Contextual menu anchored to an element.            | ◻ |
| **Loading**       | `IonLoading`        | Blocking spinner with a message during an op.      | ◻ |
| **Picker**        | `IonPicker`         | Wheel-style value selection.                       | ◻ |

## Navigation

| Widget            | Ionic component                    | Use for                               | Status |
| ----------------- | ---------------------------------- | ------------------------------------- | ------ |
| **Tabs**          | `IonTabs` / `IonTabBar`            | Top-level sections (Loads, Messages). | ✅ |
| **Router outlet** | `IonRouterOutlet`                  | Stacked list → detail navigation.     | ✅ |
| **Back button**   | `IonBackButton`                    | Return to the previous view.          | ✅ |
| **Side menu**     | `IonMenu`                          | Slide-in drawer (settings, profile).  | ◻ |
| **Segment**       | `IonSegment`                       | Switch sub-views within a page (tabs-lite). | ◻ |
| **Breadcrumbs**   | `IonBreadcrumbs`                   | Deep hierarchy trails (desktop-ish).  | ◻ |

## Lists & data

| Widget               | Ionic component       | Use for                                    | Status |
| -------------------- | --------------------- | ------------------------------------------ | ------ |
| **List / Item**      | `IonList` / `IonItem` | Rows; `routerLink` + `detail` for drill-in.| ✅ |
| **Sliding item**     | `IonItemSliding`      | Swipe actions (archive, delete, mark read).| ◻ |
| **Pull to refresh**  | `IonRefresher`        | Refresh a list by pulling down.            | ◻ |
| **Infinite scroll**  | `IonInfiniteScroll`   | Load more on scroll (message history).     | ◻ |
| **Reorder**          | `IonReorderGroup`     | Drag to reorder (e.g. trip waypoints).     | ◻ |
| **Accordion**        | `IonAccordionGroup`   | Collapsible sections.                      | ◻ |

## Inputs & controls

| Widget         | Ionic component | Use for                              | Status |
| -------------- | --------------- | ------------------------------------ | ------ |
| **Input**      | `IonInput`      | Single-line text (composer).         | ✅ |
| **Textarea**   | `IonTextarea`   | Multi-line text.                     | ◻ |
| **Searchbar**  | `IonSearchbar`  | Filter lists.                        | ◻ |
| **Select**     | `IonSelect`     | Pick from options (lookup values).   | ◻ |
| **Checkbox / Radio / Toggle** | `IonCheckbox` / `IonRadio` / `IonToggle` | Booleans & choices. | ◻ |
| **Range**      | `IonRange`      | Sliders (e.g. radius).               | ◻ |
| **Datetime**   | `IonDatetime`   | Date/time selection (schedules).     | ◻ |

## Status, layout & content

| Widget            | Ionic component   | Use for                                  | Status |
| ----------------- | ----------------- | ---------------------------------------- | ------ |
| **Chip**          | `IonChip`         | Compact tags / attachments.              | ✅ |
| **Note**          | `IonNote`         | Secondary/metadata text.                 | ✅ |
| **Card**          | `IonCard`         | Grouped content blocks.                  | ◻ |
| **FAB**           | `IonFab`          | Floating action button (primary action). | ◻ |
| **Badge**         | `IonBadge`        | Counts (unread messages).                | ◻ |
| **Avatar / Thumbnail** | `IonAvatar` / `IonThumbnail` | Profile pics, doc previews.  | ◻ |
| **Spinner**       | `IonSpinner`      | Inline loading indicator.                | ◻ |
| **Skeleton**      | `IonSkeletonText` | Placeholder while loading.               | ◻ |
| **Progress bar**  | `IonProgressBar`  | Determinate/indeterminate progress (uploads). | ◻ |
| **Grid**          | `IonGrid`         | Responsive columns.                      | ◻ |

## Conventions (KISS)

- Drill-down uses **list → detail** via `routerLink` + `IonBackButton`, not
  in-page show/hide.
- **Toast** for transient, non-blocking feedback; **Alert** only for decisions;
  **Modal** for multi-field tasks.
- Show a **Badge** for unread counts; a **Spinner/Skeleton** while loading.
- Reserve **FAB** for the single primary action on a screen.

## Planned UX pattern: clickable message toasts

While a driver is on another view (e.g. the map), an incoming message surfaces as
an `IonToast` (2–3s) with a tap handler that routes to `/messages/:channelId`;
the destination's `IonBackButton` returns them to where they were. Requires
realtime/polling delivery (tracked in `TODO.md`).
