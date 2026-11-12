#!/usr/bin/env python3
"""Generate the Dainvo ribbon command icons (one line-icon set, light and dark).

Every command on the Dainvo ribbons gets a 1.5-1.75 px stroke icon drawn from
Lucide (ISC licence, see LUCIDE-LICENSE) or, where Lucide has nothing fitting,
a path in the same style from the design board. Writes cmd/sc_<name>.svg (16 px),
cmd/lc_<name>.svg (24 px) and cmd/32/<name>.svg into colibre_svg and
colibre_dark_svg, removes the per-language variants of those commands, and drops
links.txt entries that would redirect them to the old icons.

Run from the repository root:  python3 dainvo/icons/ribbon_icons.py
Lucide is read from DAINVO_LUCIDE_DIR (the lucide-react dist/esm/icons folder).
"""
import glob
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LUCIDE = os.environ.get('DAINVO_LUCIDE_DIR', os.path.expanduser(
    '~/Projects/Dainvo Project/dainvo-docs/node_modules/.pnpm/lucide-react@1.23.0_react@19.2.7/'
    'node_modules/lucide-react/dist/esm/icons'))

# theme -> (stroke, accent, links.txt that feeds it)
THEMES = {
    'colibre_svg': ('#262626', '#4F46E5', 'colibre'),
    'colibre_dark_svg': ('#ECECEC', '#8B93FF', 'colibre_dark'),
}
# size prefix -> (pixel size, stroke width in 24-unit space)
SIZES = {'sc_': (16, 1.75), 'lc_': (24, 1.5), '32/': (32, 1.5)}

# Paths from the Dainvo ribbon design for things Lucide does not draw.
D = {
    'fontcolor': 'M6 17 12 4l6 13M8.2 12.5h7.6',
    'margins': 'M4 3h16v18H4zM8 3v18M16 3v18M4 7h16M4 17h16',
    'hyphen': 'M3 7h8M3 12h18M3 17h8M15 17h3',
    'linenumbers': 'M10 6h10M10 12h10M10 18h10M5 4v4M4 12h2l-2 2h2M4 17h2v3H4',
    'linespacing': 'M11 6h10M11 12h10M11 18h10M5 4v16M3 6l2-2 2 2M3 18l2 2 2-2',
    'footnote': 'M4 5h16M4 10h16M4 15h9M4 20h5M18 13v7',
    'endnote': 'M4 5h16M4 10h16M4 15h16M4 20h5M16 20h4',
    'crossref': 'M4 4h7v7H4zM13 13h7v7h-7zM11 7h3a3 3 0 0 1 3 3v3',
    'indexentry': 'M12 2v20M5 8h6M13 16h6',
    'outline': 'M3 5h.01M8 5h13M7 12h.01M12 12h9M11 19h.01M16 19h5',
    'pagebreak': 'M6 3v6h12V3M6 21v-6h12v6M3 12h2M8 12h2M14 12h2M19 12h2',
    'breaks': 'M6 3v5h12V3M6 21v-5h12v5M3 12h18',
    'condformat': 'M3 3h18v18H3zM3 9h18M9 21V9M13 14h5M13 17h3',
    'freeze': 'M3 3h18v18H3zM3 9h18M9 3v18M14 14l4 4M18 14l-4 4',
    'pivot': 'M3 3h18v18H3zM3 9h18M9 3v18M13 14l3 3-3 3',
    'onehundred': 'M4 8l2-1v10M9 7h3v10H9zM15 7h3v10h-3z',
    'pagewidth': 'M3 5h18v14H3zM7 12h10M9 10l-2 2 2 2M15 10l2 2-2 2',
    'multipage': 'M3 4h8v16H3zM13 4h8v16h-8z',
    'styles': 'M4 6h16M4 12h10M4 18h7M17 13l3 3-5 5h-3v-3z',
    'newslide': 'M3 4h18v12H3zM12 7v6M9 10h6M8 20h8',
    'fromstart': 'M4 4v16M8 4l12 8-12 8z',
    'shadow': 'M4 4h12v12H4zM8 20h12V8',
    'linecolor': 'M4 16 16 4l3 3L7 19H4z',
    'above': 'M4 3h16M12 7v10M8 11l4-4 4 4M4 21h16',
    'below': 'M4 3h16M12 7v10M8 13l4 4 4-4M4 21h16',
    'shear': 'M8 5h13l-5 14H3z',
    'slant': 'M4 19c2-9 5-14 8-14s6 5 8 14',
    'delrows': 'M3 4h18v6H3zM9 15l6 6M15 15l-6 6',
    'delcols': 'M4 3h6v18H4zM15 9l6 6M21 9l-6 6',
    'deltable': 'M3 3h12v12H3zM3 9h12M9 3v12M17 17l4 4M21 17l-4 4',
    'linestyle': 'M3 6h18M3 12h4M10 12h4M17 12h4M3 18h1M8 18h1M13 18h1M18 18h1',
    'linewidth': 'M3 5h18M3 11h18M3 18h18',
    'wrapoff': 'M3 4h18M8 9h8v6H8zM3 20h18',
    'wrapon': 'M3 5h18M3 19h18M8 9h8v6H8zM3 9h2M3 12h2M3 15h2M19 9h2M19 12h2M19 15h2',
    'wrapideal': 'M3 5h18M3 19h18M9 9h6v6H9zM3 9h3M3 12h3M3 15h3M18 9h3M18 12h3M18 15h3',
    'wrapleft': 'M3 5h18M3 19h18M13 9h8v6h-8zM3 9h6M3 12h6M3 15h6',
    'wrapright': 'M3 5h18M3 19h18M3 9h8v6H3zM15 9h6M15 12h6M15 15h6',
    'wrapthrough': 'M3 6h18M3 12h18M3 18h18M8 8h8v8H8z',
}

# command -> icon: a Lucide name, 'd:<key in D>', with '^' to keep the glyph out
# of the colour bar LibreOffice paints along the bottom of colour buttons.
ICONS = {
    'AboveSpacing': 'd:above', 'BelowSpacing': 'd:below',
    'AcceptChanges': 'file-check', 'AcceptTrackedChange': 'check', 'AcceptTrackedChanges': 'check-check',
    'RejectTrackedChange': 'x', 'NextTrackedChange': 'chevron-right', 'PreviousTrackedChange': 'chevron-left',
    'AddName': 'tag', 'DefineName': 'tags', 'InsertName': 'tag',
    'AlignLeft': 'text-align-start', 'AlignHorizontalCenter': 'text-align-center', 'AlignRight': 'text-align-end',
    'AlignTop': 'align-vertical-justify-start', 'AlignVCenter': 'align-vertical-justify-center',
    'AlignBottom': 'align-vertical-justify-end',
    'LeftPara': 'text-align-start', 'CenterPara': 'text-align-center', 'RightPara': 'text-align-end',
    'JustifyPara': 'text-align-justify',
    'AssignLayout': 'layout-template', 'AttributePageSize': 'file', 'AutoSum': 'sigma',
    'BackgroundColor': '^paint-bucket', 'CharBackColor': '^highlighter', 'Color': '^d:fontcolor',
    'XLineColor': '^d:linecolor', 'FillShadow': 'd:shadow',
    'BasicShapes': 'shapes', 'BibliographyComponent': 'library-big', 'Bold': 'bold', 'Italic': 'italic',
    'Underline': 'underline', 'Strikeout': 'strikethrough', 'SubScript': 'subscript', 'SuperScript': 'superscript',
    'BrowseView': 'globe', 'Calculate': 'calculator', 'CharmapControl': 'omega', 'ClearArrows': 'eraser',
    'CompareDocuments': 'git-compare', 'ConditionalFormatMenu': 'd:condformat', 'ControlCodes': 'pilcrow',
    'Copy': 'copy', 'Cut': 'scissors', 'Paste': 'clipboard-paste', 'FormatPaintbrush': 'paintbrush',
    'ResetAttributes': 'remove-formatting', 'Grow': 'a-arrow-up', 'Shrink': 'a-arrow-down',
    'Undo': 'undo-2', 'Redo': 'redo-2',
    'CustomAnimation': 'sparkles', 'CustomShowDialog': 'list-video', 'DataAreaRefresh': 'refresh-cw',
    'DataDataPilotRun': 'd:pivot', 'DataFilterAutoFilter': 'funnel', 'DataFilterRemoveFilter': 'funnel-x',
    'DataFilterStandardFilter': 'list-filter', 'DataSort': 'arrow-down-a-z', 'DataSubTotals': 'square-sigma',
    'DecrementIndent': 'list-indent-decrease', 'IncrementIndent': 'list-indent-increase',
    'DefaultBullet': 'list', 'DefaultNumbering': 'list-ordered', 'SetOutline': 'd:outline',
    'DeleteAllNotes': 'message-square-x', 'DeleteAnnotation': 'message-square-x', 'DeleteComment': 'message-square-x',
    'DeleteNote': 'message-square-x', 'InsertAnnotation': 'message-square-plus',
    'NextAnnotation': 'chevron-right', 'PreviousAnnotation': 'chevron-left',
    'ShowAllNotes': 'messages-square', 'ShowAnnotations': 'messages-square', 'HideAllNotes': 'message-square-off',
    'DeleteCell': 'grid-2x2-x', 'InsertCell': 'grid-2x2-plus', 'DesignerDialog': 'd:styles',
    'DiaMode': 'layout-grid', 'DrawText': 'type', 'Text': 'type', 'DuplicateSlide': 'copy-plus',
    'EditHeaderAndFooter': 'panel-top', 'HeaderAndFooter': 'panel-top', 'InsertPageHeader': 'panel-top',
    'InsertPageFooter': 'panel-bottom', 'FormatGroup': 'group', 'Group': 'group', 'Ungroup': 'ungroup',
    'FreezePanes': 'd:freeze', 'FullScreen': 'maximize', 'FunctionDialog': 'square-function',
    'GridVisible': 'grid-3x3', 'ToggleSheetGrid': 'grid-3x3', 'HandleDuplicateRecords': 'copy-x',
    'HideSlide': 'eye-off', 'ShowSlide': 'eye', 'HyperlinkDialog': 'link', 'Hyphenate': 'd:hyphen',
    'InputLineVisible': 'text-cursor-input', 'InsertAVMedia': 'clapperboard',
    'InsertAuthoritiesEntry': 'quote', 'InsertBookmark': 'bookmark', 'InsertBreak': 'd:breaks',
    'InsertCaptionDialog': 'captions', 'InsertEndnote': 'd:endnote', 'InsertFootnote': 'd:footnote',
    'InsertExternalDataSource': 'cloud-download', 'InsertFieldCtrl': 'braces', 'InsertGraphic': 'image',
    'InsertIndexesEntry': 'd:indexentry', 'InsertMultiIndex': 'table-of-contents', 'InsertObjectChart': 'chart-column',
    'InsertObjectStarMath': 'radical', 'InsertPageNumberField': 'file-digit', 'InsertPagebreak': 'd:pagebreak',
    'InsertReferenceField': 'd:crossref', 'InsertSlide': 'd:newslide', 'InsertTable': 'table',
    'LineNumberingDialog': 'd:linenumbers', 'LineSpacing': 'd:linespacing', 'Navigator': 'compass',
    'NormalMultiPaneGUI': 'panel-left', 'NormalViewMode': 'sheet', 'NotesMode': 'notebook-text',
    'OutlineMode': 'list-tree', 'NumberFormatCurrency': 'banknote', 'NumberFormatPercent': 'percent',
    'NumberFormatDecDecimals': 'decimals-arrow-left', 'NumberFormatIncDecimals': 'decimals-arrow-right',
    'ObjectAlign': 'align-horizontal-distribute-center', 'ObjectBackOne': 'send-to-back',
    'ObjectForwardOne': 'bring-to-front', 'ObjectPosition': 'move', 'Orientation': 'rotate-ccw-square',
    'PageColumnType': 'columns-2', 'PageMargin': 'd:margins', 'PagebreakMode': 'scissors-line-dashed',
    'Presentation': 'd:fromstart', 'PresentationCurrentSlide': 'play', 'PresentationDialog': 'monitor-cog',
    'PrintLayout': 'file-text', 'Protect': 'lock', 'ProtectTraceChangeMode': 'file-lock',
    'ToolProtectionDocument': 'shield', 'RehearseTimings': 'timer', 'RotateRight': 'rotate-cw', 'Ruler': 'ruler',
    'SearchDialog': 'search', 'SelectBackground': 'wallpaper', 'SetBorderStyle': 'grid-2x2',
    'ShowDependents': 'git-fork', 'ShowPrecedents': 'git-merge', 'ShowErrors': 'circle-alert',
    'ShowMultiplePages': 'd:multipage', 'ShowTrackedChanges': 'file-diff', 'Sidebar': 'panel-right',
    'SlideChangeWindow': 'blend', 'SlideMasterPage': 'layout-panel-top', 'SlideSetup': 'ratio',
    'SpellDialog': 'spell-check', 'SpellingAndGrammarDialog': 'spell-check', 'SpellOnline': 'spell-check-2',
    'SplitWindow': 'square-split-horizontal', 'TextToColumns': 'columns-3', 'TextWrap': 'wrap-text',
    'WrapText': 'wrap-text', 'ThemeDialog': 'palette', 'ThesaurusDialog': 'book-a',
    'ToggleMergeCells': 'table-cells-merge', 'TraceChangeMode': 'file-pen-line', 'TrackChanges': 'file-pen-line',
    'UpdateAllIndexes': 'refresh-cw', 'UpdateCurIndex': 'refresh-ccw', 'Validation': 'list-checks',
    'ViewRowColumnHeaders': 'table-2', 'WordCountDialog': 'whole-word', 'Zoom': 'zoom-in',
    'Zoom100Percent': 'd:onehundred', 'ZoomPage': 'scan', 'ZoomPageWidth': 'd:pagewidth',
    'StylesPreview': 'd:styles',
    # sidebar deck tabs (Sidebar.xcu and the symphony links point at these)
    'ConfigureDialog': 'sliders-horizontal', 'Gallery': 'images', 'DiaEffect': 'sparkles', 'InsertMath': 'sigma',
    'DBViewFunctions': 'square-function', 'AddDirect': 'file-plus', 'InspectorDeck': 'scan-search',
    'RecSearch': 'search', 'TrackChangesBar': 'file-diff',
    # contextual tabs (Table, Image, Draw, Form, Media, Outline, Print, Master, 3D ...)
    'AVMediaPlayer': 'circle-play', 'AVMediaToolBox': 'film', 'AdvancedMode': 'move-3d',
    'AlignCenter': 'align-horizontal-justify-center', 'AlignUp': 'align-vertical-justify-start',
    'AlignMiddle': 'align-vertical-justify-center', 'AlignDown': 'align-vertical-justify-end',
    'ObjectAlignLeft': 'align-horizontal-justify-start', 'ObjectAlignRight': 'align-horizontal-justify-end',
    'CellVertTop': 'align-vertical-justify-start', 'CellVertCenter': 'align-vertical-justify-center',
    'CellVertBottom': 'align-vertical-justify-end',
    'AnchorMenu': 'anchor', 'AnimationEffects': 'sparkle', 'AssignMacro': 'code', 'AutoControlFocus': 'focus',
    'AutoFormat': 'wand-sparkles', 'TableDesign': 'swatch-book', 'UseWizards': 'wand',
    'BorderDialog': 'grid-2x2', 'TableBoundaries': 'square-dashed', 'Break': 'd:breaks',
    'BringToFront': 'arrow-up-to-line', 'SendToBack': 'arrow-down-to-line',
    'ChainFrames': 'link-2', 'UnhainFrames': 'link-2-off', 'EditHyperlink': 'link', 'RemoveHyperlink': 'unlink',
    'OpenHyperlinkOnCursor': 'external-link',
    'ChangeBezier': 'pen-tool', 'ChangePolygon': 'pentagon', 'ToggleObjectBezierMode': 'spline',
    'convert_to_contour': 'squircle', 'ContourDialog': 'lasso', 'ToggleObjectRotateMode': 'rotate-3d',
    'ChangePicture': 'image-up', 'CompressGraphic': 'minimize-2', 'Crop': 'crop', 'ExternalEdit': 'square-arrow-out-up-right',
    'GraphicDialog': 'image', 'ImageControl': 'image', 'ImageMapDialog': 'map', 'OriginalSize': 'scaling',
    'SaveGraphic': 'download', 'SaveBackground': 'save', 'GraphicFilterToolbox': 'aperture', 'GrafMode': 'image',
    'GrafContrast': 'contrast', 'GrafGamma': 'sun-medium', 'GrafLuminance': 'sun', 'GrafTransparence': 'droplet',
    'ColorSettings': 'pipette', 'ObjectTitleDescription': 'text-quote', 'RenameObject': 'pencil-line',
    'CheckBox': 'square-check', 'ComboBox': 'square-chevron-down', 'ListBox': 'list', 'Label': 'tag',
    'Pushbutton': 'rectangle-horizontal', 'RadioButton': 'circle-dot', 'GroupBox': 'group',
    'Imagebutton': 'square-mouse-pointer', 'FileControl': 'file-up', 'DateField': 'calendar', 'TimeField': 'clock',
    'NumericField': 'hash', 'CurrencyField': 'circle-dollar-sign', 'PatternField': 'asterisk',
    'FormattedField': 'text-cursor-input', 'NavigationBar': 'arrow-left-right', 'ControlProperties': 'settings-2',
    'FormProperties': 'clipboard-list', 'SwitchControlDesignMode': 'pencil-ruler', 'ShowFmExplorer': 'folder-tree',
    'Combine': 'combine', 'Merge': 'squares-unite', 'Substract': 'squares-subtract', 'Intersect': 'squares-intersect',
    'Dismantle': 'puzzle', 'Connect': 'cable',
    'ConvertInto3D': 'box', 'ConvertInto3DLatheFast': 'cylinder', 'ConvertIntoBitmap': 'image-down',
    'ConvertIntoMetaFile': 'file-image', 'ConvertTableToText': 'text', 'ConvertTextToTable': 'grid-3x2',
    'CopyObjects': 'copy', 'CrookSlant': 'd:slant', 'Shear': 'd:shear', 'TransformDialog': 'scale-3d',
    'DeleteRows': 'd:delrows', 'DeleteColumns': 'd:delcols', 'DeleteTable': 'd:deltable',
    'InsertRowsBefore': 'between-horizontal-start', 'InsertRowsAfter': 'between-horizontal-end',
    'InsertColumnsBefore': 'between-vertical-start', 'InsertColumnsAfter': 'between-vertical-end',
    'InsertRowDialog': 'between-horizontal-start', 'InsertColumnDialog': 'between-vertical-start',
    'EntireRow': 'rows-3', 'EntireColumn': 'columns-3', 'EntireCell': 'square', 'SelectTable': 'square-dashed-mouse-pointer',
    'DistributeColumns': 'align-horizontal-space-around', 'DistributeRows': 'align-vertical-space-around',
    'DistributeSelection': 'align-horizontal-space-between',
    'EqualizeWidth': 'move-horizontal', 'EqualizeHeight': 'move-vertical',
    'SetColumnWidth': 'arrow-left-right', 'SetRowHeight': 'arrow-up-down',
    'SetOptimalColumnWidth': 'unfold-horizontal', 'SetOptimalRowHeight': 'unfold-vertical',
    'SetMinimalColumnWidth': 'fold-horizontal', 'SetMinimalRowHeight': 'fold-vertical', 'FitCellSize': 'expand',
    'MergeCells': 'table-cells-merge', 'SplitCell': 'table-cells-split', 'MergeTable': 'merge', 'SplitTable': 'split',
    'RowSplit': 'rows-2', 'HeadingRowsRepeat': 'repeat', 'OptimizeTable': 'ruler-dimension-line',
    'TableDialog': 'table-properties', 'TableNumberFormatDialog': 'hash', 'TableNumberRecognition': 'scan-text',
    'TableSort': 'arrow-down-a-z', 'TableCellBackgroundColor': '^paint-bucket', 'InsertFormula': 'variable',
    'UnsetCellsReadOnly': 'lock-open',
    'Edit': 'pencil', 'EndPara': 'pilcrow-right', 'StartPara': 'pilcrow-left', 'ParaRightToLeft': 'arrow-left-to-line',
    'EnterGroup': 'log-in', 'LeaveGroup': 'log-out', 'NameGroup': 'tag', 'FormatUngroup': 'ungroup',
    'ExtrusionToggle': 'box', 'Extrusion3DColor': 'paint-bucket', 'ExtrusionDepthFloater': 'layers',
    'ExtrusionDirectionFloater': 'axis-3d', 'ExtrusionSurfaceFloater': 'sun', 'Window3D': 'container',
    'ExtrusionTiltUp': 'arrow-up', 'ExtrusionTiltDown': 'arrow-down', 'ExtrusionTiltLeft': 'arrow-left',
    'ExtrusionTiltRight': 'arrow-right',
    'FillStyle': 'paint-bucket', 'FormatArea': 'paint-bucket', 'FormatLine': 'pen-line', 'FrameLineColor': '^d:linecolor',
    'Line': 'slash', 'LineEndStyle': 'move-right', 'LineStyle': 'd:linestyle', 'XLineStyle': 'd:linestyle',
    'LineWidth': 'd:linewidth', 'FrameDialog': 'frame',
    'FlipHorizontal': 'flip-horizontal-2', 'FlipVertical': 'flip-vertical-2',
    'MirrorHorz': 'flip-horizontal-2', 'MirrorVert': 'flip-vertical-2',
    'FirstPage': 'chevron-first', 'LastPage': 'chevron-last', 'NextPage': 'chevron-right',
    'PreviousPage': 'chevron-left', 'PageUp': 'chevron-up', 'PageDown': 'chevron-down',
    'GoToStartOfDoc': 'chevrons-up', 'GoToEndOfDoc': 'chevrons-down', 'JumpToSpecificPage': 'file-input',
    'Grid': 'grid-3x3', 'GridUse': 'magnet', 'HelplinesVisible': 'crosshair', 'HelplinesUse': 'magnet',
    'HandoutMode': 'files', 'NotesMasterPage': 'notebook-pen', 'MasterLayouts': 'layout-dashboard',
    'InsertMasterPage': 'square-plus', 'DeleteMasterPage': 'square-x', 'RenameMasterPage': 'pencil-line',
    'CloseMasterView': 'circle-x', 'ClosePreview': 'circle-x', 'LeftPaneImpress': 'panel-left-open',
    'LeftParaMargin': 'arrow-right-from-line', 'RightParaMargin': 'arrow-left-from-line',
    'Margins': 'd:margins', 'PageFormatDialog': 'file-cog', 'TabDialog': 'arrow-right-to-line', 'Menubar': 'menu',
    'Morphing': 'route', 'NumberFormatDate': 'calendar', 'NumberFormatDecimal': 'hash', 'NumberFormatType': 'list-ordered',
    'ObjectMenue': 'square-menu', 'OpenReadOnly': 'book-lock',
    'OutlineUp': 'arrow-up', 'OutlineDown': 'arrow-down', 'OutlineLeft': 'arrow-left', 'OutlineRight': 'arrow-right',
    'OutlineExpand': 'plus', 'OutlineCollapse': 'minus', 'OutlineExpandAll': 'chevrons-up-down',
    'OutlineCollapseAll': 'chevrons-down-up', 'OutlineFormat': 'a-large-small',
    'OutputQualityColor': 'rainbow', 'OutputQualityGrayscale': 'eclipse',
    'PreviewZoom': 'zoom-in', 'ZoomPlus': 'zoom-in', 'ZoomMinus': 'zoom-out', 'Print': 'printer',
    'ShowBookview': 'book-open', 'ShowSinglePage': 'file', 'ShowTwoPages': 'book-open-text', 'ShowRuler': 'ruler',
    'StatusBarVisible': 'panel-bottom-dashed', 'ToggleTabBarVisibility': 'panel-bottom',
    'TogglePrintColRowHeader': 'table-2', 'TogglePrintComments': 'message-square', 'TogglePrintGrid': 'grid-3x3',
    'TogglePrintZeroValues': 'circle-off',
    'WrapOff': 'd:wrapoff', 'WrapOn': 'd:wrapon', 'WrapIdeal': 'd:wrapideal', 'WrapLeft': 'd:wrapleft',
    'WrapRight': 'd:wrapright', 'WrapThrough': 'd:wrapthrough',
}
# other images by path (without extension) -> (icon, size)
FILES = {
    'sfx2/res/symphony/sidebar-a11y-large': ('accessibility', 24),
}
# large buttons that carry the accent colour on the ribbon
ACCENT = {'Paste', 'InsertTable', 'InsertGraphic', 'InsertObjectChart', 'InsertAnnotation', 'TrackChanges',
          'SpellingAndGrammarDialog', 'InsertMultiIndex', 'InsertSlide', 'Presentation', 'DataDataPilotRun',
          'FunctionDialog', 'InsertFootnote', 'InsertAuthoritiesEntry', 'DataSort', 'DataFilterAutoFilter'}

NODE = re.compile(r'\[\s*"(\w+)",\s*\{([^}]*)\}\s*\]')
ATTR = re.compile(r'(\w+):\s*"([^"]*)"')


def lucide(name):
    src = open(os.path.join(LUCIDE, name + '.mjs'), encoding='utf-8').read()
    if '__iconNode = ' not in src:  # renamed icon: the old name re-exports the new one
        return lucide(re.search(r"from '\./([\w-]+)\.mjs'", src).group(1))
    body = src[src.index('__iconNode = '):src.index('];\nconst')]
    out = []
    for tag, attrs in NODE.findall(body):
        kv = ' '.join(f'{k}="{v}"' for k, v in ATTR.findall(attrs) if k != 'key')
        out.append(f'<{tag} {kv}/>')
    return ''.join(out)


def shapes(spec):
    return f'<path d="{D[spec[2:]]}"/>' if spec.startswith('d:') else lucide(spec)


def svg(spec, size, width, color):
    top = spec.startswith('^')
    inner = shapes(spec.lstrip('^'))
    # colour buttons: shrink into the top three quarters, centred
    scale = size / 24 * (0.75 if top else 1)
    dx = size * 0.125 if top else 0
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">'
            f'<g transform="translate({dx:g} 0) scale({scale:.5g})" fill="none" stroke="{color}" '
            f'stroke-width="{width / (0.75 if top else 1):.3g}" stroke-linecap="round" stroke-linejoin="round">'
            f'{inner}</g></svg>\n')


def main():
    names = {cmd.lower() for cmd in ICONS}
    written = removed = 0
    for theme, (color, accent, links_theme) in THEMES.items():
        cmd_dir = os.path.join(ROOT, 'icon-themes', theme, 'cmd')
        for cmd, spec in ICONS.items():
            for prefix, (size, width) in SIZES.items():
                c = accent if cmd in ACCENT and prefix != 'sc_' else color
                path = os.path.join(cmd_dir, prefix + cmd.lower() + '.svg')
                os.makedirs(os.path.dirname(path), exist_ok=True)
                open(path, 'w', encoding='utf-8').write(svg(spec, size, width, c))
                written += 1
        for stem, (spec, size) in FILES.items():
            path = os.path.join(ROOT, 'icon-themes', theme, stem + '.svg')
            open(path, 'w', encoding='utf-8').write(svg(spec, size, SIZES['lc_'][1], color))
            written += 1
        # per-language variants (Bold as "F", "N", ...) would bring the old style back
        for f in glob.glob(os.path.join(cmd_dir, '*', '*.svg')) + glob.glob(os.path.join(cmd_dir, '32', '*', '*.svg')):
            stem = os.path.basename(f)[:-4]
            if os.path.basename(os.path.dirname(f)) != '32' and re.sub(r'^(sc_|lc_)', '', stem) in names:
                os.remove(f)
                removed += 1
        links = os.path.join(ROOT, 'icon-themes', links_theme, 'links.txt')
        lines = open(links, encoding='utf-8').read().split('\n')
        def ours(line):
            src = line.split(' ')[0]
            m = re.match(r'cmd/(?:32/)?(?:[\w-]+/)?(?:sc_|lc_)?(\w+)\.(?:png|svg)$', src)
            return bool(m) and m.group(1) in names
        keep = [l for l in lines if not ours(l)]
        open(links, 'w', encoding='utf-8').write('\n'.join(keep))
    print(f'{written} icons written, {removed} language variants removed')


if __name__ == '__main__':
    main()
