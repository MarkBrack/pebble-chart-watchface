# Chart Clock

A Pebble watchface for chart and spreadsheet nerds.

## Design

- Hours are shown as a 12-slice donut chart
- Minutes are shown as six bars, one for each 10-minute bucket
- The active hour slice and active minute bar use a highlight color
- The layout is sized for `emery`

## Build

```bash
cd samples/projects/chart-geek-watchface
pebble build
```
