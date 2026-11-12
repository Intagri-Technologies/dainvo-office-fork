#!/usr/bin/env python3
"""Generate the Dainvo Office ribbons (notebookbar .ui files) from ribbons.json.

For each app this rebuilds the main tabs of notebookbar.ui (labelled) and
notebookbar_compact.ui (icons only) from the spec, and keeps LibreOffice's
contextual tabs (Table, Image, Draw, ...) from the upstream file, minus their
group captions and per-tab menus.

Run from the repository root:  python3 dainvo/ribbon/generate.py
"""
import glob
import json
import os
import re
import subprocess
import sys
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPEC = os.path.join(ROOT, 'dainvo', 'ribbon', 'ribbons.json')
UPSTREAM_REF = 'master'

# Commands LibreOffice's own ribbons present as GtkMenuToolButton (split/dropdown
# with a built-in popup); every other command gets its arrow from its controller.
MENU_CMDS = {
    '.uno:AddDirect', '.uno:AttributePageSize', '.uno:BackgroundColor', '.uno:CharBackColor',
    '.uno:Color', '.uno:ColumnOperations', '.uno:ConditionalFormatMenu', '.uno:InsertFieldCtrl',
    '.uno:NumberFormatCurrency', '.uno:OptimizeTable', '.uno:Orientation', '.uno:PageColumnType',
    '.uno:PageMargin', '.uno:Paste', '.uno:RowOperations', '.uno:Spacing',
}


def known_commands():
    names = set()
    for f in glob.glob(os.path.join(ROOT, 'officecfg/registry/data/org/openoffice/Office/UI/*Commands.xcu')):
        names.update(re.findall(r'oor:name="(\.uno:[A-Za-z0-9]+)"', open(f, encoding='utf-8').read()))
    return names


def upstream(path):
    return subprocess.run(['git', 'show', f'{UPSTREAM_REF}:{path}'], cwd=ROOT, check=True,
                          capture_output=True, text=True).stdout.split('\n')


def ind(line):
    return len(line) - len(line.lstrip(' '))


def block_end(lines, start):
    """Index of the line closing the element opened at lines[start]."""
    tag = re.match(r'\s*<(\w+)', lines[start]).group(1)
    pad = ' ' * ind(lines[start])
    for j in range(start + 1, len(lines)):
        if lines[j] == f'{pad}</{tag}>':
            return j
    raise ValueError(f'unclosed {tag} at {start + 1}')


class Xml:
    """Tiny indented XML writer."""

    def __init__(self, base):
        self.out, self.level = [], base

    def line(self, text):
        self.out.append(' ' * self.level + text)

    def open(self, text):
        self.line(text)
        self.level += 2

    def close(self, text):
        self.level -= 2
        self.line(text)

    def props(self, **kv):
        for k, v in kv.items():
            self.line(f'<property name="{k.replace("_", "-") if k != "icon_size" else k}">{v}</property>')


class Generator:
    def __init__(self, app, spec, commands):
        self.app, self.spec, self.commands = app, spec, commands
        self.uid = 0
        self.menu_ids = []
        self.overflow = []      # commands hidden in the icons-only layout (current tab)
        self.menus = []         # (menu id, commands) for each tab's More menu

    def next_id(self, prefix):
        self.uid += 1
        return f'dv{prefix}{self.uid}'

    def parse(self, item):
        cmd, *flags = item.split('|')
        if cmd not in self.commands:
            sys.exit(f'{self.app}: unknown command {cmd}')
        return cmd, 'o' in flags

    def toolbox(self, x, items, style, large=False, expand=False):
        x.open(f'<object class="sfxlo-NotebookbarToolBox" id="{self.next_id("tb")}">')
        x.props(visible='True', can_focus='False')
        if expand:
            x.props(hexpand='True', vexpand='True')
        x.props(toolbar_style=style, show_arrow='False')
        if large:
            x.props(icon_size='3')
        for cmd in items:
            klass = 'GtkMenuToolButton' if cmd in MENU_CMDS else 'GtkToolButton'
            bid = self.next_id('btn')
            self.menu_ids.append((klass, bid))
            x.open('<child>')
            x.open(f'<object class="{klass}" id="{bid}">')
            x.props(visible='True', action_name=escape(cmd))
            x.close('</object>')
            x.open('<packing>')
            x.props(expand='True' if expand else 'False', homogeneous='True' if expand else 'False')
            x.close('</packing>')
            x.close('</child>')
        x.close('</object>')

    def child(self, x, build, expand=False):
        x.open('<child>')
        build()
        x.open('<packing>')
        x.props(expand='True' if expand else 'False', fill='True')
        x.close('</packing>')
        x.close('</child>')

    def group(self, x, g, compact, priority=None, lead_separator=False):
        def body():
            # VclOptionalBox: the tab's PriorityHBox hides low-priority groups
            # (right-most first) when the window is too narrow
            x.open(f'<object class="VclOptionalBox" id="{self.next_id("grp")}">')
            x.props(visible='True', can_focus='False', valign='center', spacing='2')
            if priority is not None:
                x.open('<style>')
                x.line(f'<class name="priority-{priority}"/>')
                x.close('</style>')
            if lead_separator:
                self.separator(x)
            if compact:
                parsed = list(map(self.parse, g.get('L', []) + sum(g.get('R', []), []) + g.get('L2', [])))
                items = [c for c, over in parsed if not over]
                self.overflow += [c for c, over in parsed if over]
                if g.get('G') and self.spec.get('compactGallery'):
                    items.insert(len(g.get('L', [])) + len(sum(g.get('R', []), [])), self.spec['compactGallery'])
                if items:
                    self.child(x, lambda: self.toolbox(x, items, 'icons'))
            else:
                for c in g.get('L', []):
                    self.child(x, lambda c=c: self.toolbox(x, [self.parse(c)[0]], 'both', large=True))
                if g.get('R'):
                    def rows():
                        x.open(f'<object class="GtkBox" id="{self.next_id("rows")}">')
                        x.props(visible='True', can_focus='False', valign='center', orientation='vertical', spacing='2')
                        for row in g['R']:
                            cmds = [self.parse(c)[0] for c in row]
                            self.child(x, lambda cmds=cmds: self.toolbox(x, cmds, 'icons'))
                        x.close('</object>')
                    self.child(x, rows)
                if g.get('G'):
                    self.child(x, lambda: self.toolbox(x, [self.parse(g['G'])[0]], 'icons', expand=True), expand=True)
                for c in g.get('L2', []):
                    self.child(x, lambda c=c: self.toolbox(x, [self.parse(c)[0]], 'both', large=True))
            x.close('</object>')
        self.child(x, body, expand=bool(g.get('G')) and not compact)

    def more_button(self, x, cmds):
        menu_id = f'dvMenuMore{self.uid}'
        self.menus.append((menu_id, cmds))
        def body():
            x.open(f'<object class="svtlo-ManagedMenuButton" id="{self.next_id("more")}:{menu_id}">')
            x.line('<property name="label">\u00b7\u00b7\u00b7</property>')
            x.props(visible='True', can_focus='True', focus_on_click='False', receives_default='False',
                    valign='center', relief='none')
            x.close('</object>')
        self.child(x, body)

    def menu_xml(self):
        x = Xml(2)
        for menu_id, cmds in self.menus:
            x.open(f'<object class="GtkMenu" id="{menu_id}">')
            x.props(visible='True', can_focus='False')
            for c in cmds:
                mid = self.next_id('mi')
                self.menu_ids.append(('GtkMenuItem', mid))
                x.open('<child>')
                x.open(f'<object class="GtkMenuItem" id="{mid}">')
                x.props(visible='True', can_focus='False', action_name=escape(c))
                x.close('</object>')
                x.close('</child>')
            x.close('</object>')
        return x.out

    def separator(self, x):
        def body():
            x.open(f'<object class="GtkSeparator" id="{self.next_id("sep")}">')
            x.props(visible='True', can_focus='False', margin_start='6', margin_end='6', orientation='vertical')
            x.close('</object>')
        self.child(x, body)

    def tab(self, x, name, groups, pos, compact, label_ctx, label_classes):
        x.open('<child>')
        x.open(f'<object class="sfxlo-PriorityHBox" id="{name} Tab">')
        x.props(visible='True', can_focus='False', valign='center', vexpand='True')
        self.overflow = []
        for i, g in enumerate(groups):
            # the first groups (undo, clipboard) are kept longest
            self.group(x, g, compact, priority=len(groups) - i, lead_separator=i > 0)
        if compact and self.overflow:
            self.separator(x)
            self.more_button(x, list(dict.fromkeys(self.overflow)))
        x.close('</object>')
        x.open('<packing>')
        x.props(position=str(pos))
        x.close('</packing>')
        x.close('</child>')
        x.open('<child type="tab">')
        x.open(f'<object class="GtkLabel" id="{name.replace(" ", "")}Label">')
        x.props(visible='True', can_focus='False')
        x.line(f'<property name="label" translatable="yes" context="{label_ctx}">~{escape(name)}</property>')
        x.props(use_underline='True')
        if label_classes:
            x.open('<style>')
            for c in label_classes:
                x.line(f'<class name="context-{c}"/>')
            x.close('</style>')
        x.close('</object>')
        x.open('<packing>')
        x.props(position=str(pos), tab_fill='False')
        x.close('</packing>')
        x.close('</child>')


def clean_contextual(lines):
    """Drop group captions, their a11y links and per-tab menus from kept tabs."""
    removed = set()
    i = 0
    while i < len(lines):
        if '<object class="GtkLabel"' in lines[i] and lines[i - 1].strip() == '<child>':
            end = block_end(lines, i)
            if any('value="italic"' in l for l in lines[i:end]):
                removed.add(re.search(r'id="([^"]+)"', lines[i]).group(1))
                del lines[i - 1:block_end(lines, i - 1) + 1]
                i -= 1
                continue
        i += 1
    out = [l for l in lines if not (m := re.search(r'<relation type="labelled-by" target="([^"]+)"/>', l)) or m.group(1) not in removed]
    res, i = [], 0
    while i < len(out):
        if out[i].strip() == '<accessibility>' and out[i + 1].strip() == '</accessibility>':
            i += 2
            continue
        if 'class="sfxlo-DropdownBox"' in out[i] and '<property name="visible">True</property>' in out[i + 1]:
            res.append(out[i])
            res.append(out[i + 1].replace('True', 'False'))
            i += 2
            continue
        res.append(out[i])
        i += 1
    return res


def build(app, spec, commands):
    src = upstream(spec['file'])
    # commands LibreOffice's own ribbon already uses are known to work too
    commands = commands | set(re.findall(r'action-name">(\.uno:[A-Za-z0-9]+)', '\n'.join(src)))
    cc = next(i for i, l in enumerate(src) if 'id="ContextContainer"' in l)
    cc_end = block_end(src, cc)
    child_pad = ' ' * (ind(src[cc]) + 2)
    first = next(i for i in range(cc + 1, cc_end) if src[i] == child_pad + '<child>')

    # walk page/tab pairs
    contextual, label_ctx, home_classes = [], {}, None
    i = first
    while i < cc_end:
        if src[i] in (child_pad + '<child>', child_pad + '<child type="tab">'):
            end = block_end(src, i)
            if src[i].endswith('<child>'):
                page = src[i:end + 1]
            else:
                tab = src[i:end + 1]
                classes = [m.group(1) for l in tab for m in [re.search(r'class name="context-([\w]+)"', l)] if m]
                lab = next((re.search(r'context="([^"]+)">~?([^<]+)<', l) for l in tab if 'name="label"' in l), None)
                if lab:
                    label_ctx[lab.group(2).replace('~', '')] = lab.group(1)
                if 'default' in classes:
                    home_classes = classes
                elif classes:
                    contextual.append((page, tab))
            i = end + 1
        else:
            i += 1

    for compact, path in ((False, spec['file']), (True, spec['compact'])):
        gen = Generator(app, spec, commands)
        x = Xml(len(child_pad))
        pos = 0
        for n, (name, groups) in enumerate(spec['tabs']):
            ctx = label_ctx.get(name, f'Dainvo{spec["context"]}|{name.replace(" ", "")}TabLabel')
            gen.tab(x, name, groups, pos, compact, ctx, home_classes if n == 0 else None)
            pos += 1
        kept = []
        for page, tab in contextual:
            block = clean_contextual(page + tab)
            block = [re.sub(r'(<property name="position">)\d+(</property>)', rf'\g<1>{pos}\g<2>', l)
                     if l.strip().startswith('<property name="position">') and ind(l) == len(child_pad) + 4 else l
                     for l in block]
            kept += block
            pos += 1
        result = src[:first] + x.out + kept + src[last_child_end(src, first, cc_end, child_pad) + 1:]
        # top-level menus for the More buttons go before the first top-level object
        top = next(i for i, l in enumerate(result) if l.startswith('  <object '))
        result = prune_menus(result[:top] + gen.menu_xml() + result[top:])
        open(os.path.join(ROOT, path), 'w', encoding='utf-8').write('\n'.join(result))
        gen_ids = gen.menu_ids
        update_suppr(spec['suppr'], path, gen_ids, spec['file'] if compact else None)
        false_list = spec['suppr'][:-len('.suppr')] + '.false'
        if compact and os.path.exists(os.path.join(ROOT, false_list)):
            update_suppr(false_list, path, [], spec['file'])
        print(f'{path}: {len(spec["tabs"])} tabs, {len(contextual)} contextual tabs kept')


def prune_menus(lines):
    """Drop top-level GtkMenus nothing refers to (upstream per-tab menus whose
    buttons the ribbon no longer has, and the obsolete commands inside them)."""
    while True:
        tops = [(i, re.search(r'id="([^"]+)"', l).group(1)) for i, l in enumerate(lines)
                if l.startswith('  <object class="GtkMenu" ')]
        dead = []
        for i, menu_id in tops:
            end = block_end(lines, i)
            rest = '\n'.join(lines[:i] + lines[end + 1:])
            ref = escape(menu_id, {'"': '&quot;'})
            if f'>{ref}</property>' not in rest and f':{ref}">' not in rest:
                dead.append((i, end))
        if not dead:
            return lines
        for i, end in reversed(dead):
            del lines[i:end + 1]


def last_child_end(src, first, cc_end, child_pad):
    last = first
    i = first
    while i < cc_end:
        if src[i].startswith(child_pad + '<child'):
            last = block_end(src, i)
            i = last + 1
        else:
            i += 1
    return last


def update_suppr(suppr, path, ids, mirror_from=None):
    f = os.path.join(ROOT, suppr)
    lines = open(f, encoding='utf-8').read().split('\n')
    generated = lambda l: "@id='dv" in l or 'btnDainvo' in l
    if mirror_from:
        # the icons-only file shares the labelled file's menus and contextual tabs
        keep = [l for l in lines if not l.startswith(path + '://')]
        keep += [path + l[len(mirror_from):] for l in lines if l.startswith(mirror_from + '://') and not generated(l)]
    else:
        keep = [l for l in lines if not (l.startswith(path + '://') and generated(l))]
    # suppressions for widgets that are no longer in the file
    present = set(re.findall(r'id="([^"]+)"', open(os.path.join(ROOT, path), encoding='utf-8').read()))
    keep = [l for l in keep if not l.startswith(path + '://')
            or not (m := re.search(r"@id='([^']+)'", l)) or m.group(1) in present]
    while keep and keep[-1] == '':
        keep.pop()
    keep += [f"{path}://{k}[@id='{i}'] button-no-label" for k, i in ids]
    open(f, 'w', encoding='utf-8').write('\n'.join(keep) + '\n')


def main():
    spec = json.load(open(SPEC, encoding='utf-8'))
    commands = known_commands()
    for app, s in spec.items():
        if not app.startswith('_'):
            build(app, s, commands)


if __name__ == '__main__':
    main()
