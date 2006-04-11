/*!
 * \file
 * <!--
 * Copyright 2003, 2004, 2006 Develer S.r.l. (http://www.develer.com/)
 * Copyright 2000 Bernardo Innocenti <bernie@codewiz.org>
 * All Rights Reserved.
 * -->
 *
 * \version $Id$
 *
 * \author Bernardo Innocenti <bernie@develer.com>
 * \author Stefano Fedrigo <aleph@develer.com>
 *
 * \brief General pourpose menu handling functions
 */

/*#*
 *#* $Log$
 *#* Revision 1.6  2006/04/11 00:07:32  bernie
 *#* Implemenent MF_SAVESEL flag.
 *#*
 *#* Revision 1.5  2006/03/22 09:49:51  bernie
 *#* Simplifications from project_grl.
 *#*
 *#* Revision 1.4  2006/03/20 17:48:35  bernie
 *#* Implement support for ROM menus.
 *#*
 *#* Revision 1.3  2006/02/20 14:34:32  bernie
 *#* Include appconfig.h before using its definitions.
 *#*
 *#* Revision 1.2  2006/02/15 09:10:51  bernie
 *#* Make title bold; Fix height when we have no menubar.
 *#*
 *#* Revision 1.1  2006/02/10 12:29:36  bernie
 *#* Add menu system.
 *#*
 *#* Revision 1.48  2005/11/27 23:02:55  bernie
 *#* Move graphics modules from mware/ to gfx/.
 *#*
 *#* Revision 1.47  2005/11/16 18:10:19  bernie
 *#* Move top-level headers to cfg/ as in DevLib.
 *#*
 *#* Revision 1.46  2005/02/17 03:49:21  bernie
 *#* Update to new PGM api.
 *#*
 *#* Revision 1.45  2005/02/11 19:11:32  aleph
 *#* Move menu_displaymsg() in new displaymsg module
 *#*
 *#* Revision 1.44  2005/01/21 20:05:57  aleph
 *#* Fix build warning with debug off
 *#*
 *#* Revision 1.43  2005/01/13 16:56:36  aleph
 *#* Fix progmem includes.
 *#*
 *#* Revision 1.42  2004/10/31 11:02:15  aleph
 *#* Rename functions with correct codying conventions; Simplify version display
 *#*
 *#* Revision 1.41  2004/10/15 17:34:33  customer_pw
 *#* Fix menuitem max length
 *#*
 *#* Revision 1.40  2004/10/06 12:55:08  customer_pw
 *#* Declare unused (if !_DEBUG) menu_count()
 *#*
 *#* Revision 1.39  2004/10/01 14:04:59  customer_pw
 *#* Add accessor functions for menu flags
 *#*
 *#* Revision 1.38  2004/09/27 12:05:46  customer_pw
 *#* Use sel label for toggle menus and remove it
 *#*
 *#* Revision 1.37  2004/09/27 10:05:33  customer_pw
 *#* Menu cosmetic fixes
 *#*
 *#* Revision 1.36  2004/09/14 22:18:03  bernie
 *#* Adapt to stricter casting rules.
 *#*
 *#* Revision 1.35  2004/09/12 17:56:03  aleph
 *#* Include debug.h instead of drv/kdebug.h
 *#*/

#include "menu.h"
#include <gfx/gfx.h>
#include <gfx/font.h>
#include <gfx/text.h>
#include <drv/kbd.h>
#include <cfg/compiler.h>
#include <cfg/debug.h>
#include <appconfig.h>
#include <string.h> /* strcpy() */

#if CPU_HARVARD
#include <avr/pgmspace.h> /* strncpy_P() */
#endif

#if CONFIG_MENU_MENUBAR
#include "menubar.h"
#endif

#if defined(CONFIG_LOCALE) && (CONFIG_LOCALE == 1)
#include "msg.h"
#else
#define PTRMSG(x) ((const char *)x)
#endif


/* Temporary fake defines for ABORT stuff... */
#define abort_top  0
#define PUSH_ABORT false
#define POP_ABORT  do {} while(0)
#define DO_ABORT   do {} while(0)


/**
 * Count the items present in a menu.
 */
static int menu_count(const struct Menu *menu)
{
	int cnt = 0;

	for (cnt = 0; /*NOP*/; ++cnt)
	{
		const MenuItem *item = &menu->items[cnt];
#if CPU_HARVARD
		MenuItem ram_item;
		if (menu->flags & MF_ROMITEMS)
		{
			memcpy_P(&ram_item, item, sizeof(ram_item));
			item = &ram_item;
		}
#endif
		if (!(item->label || item->hook))
			break;
	}

	return cnt;
}

#if 0 /* UNUSED */
/**
 * Compute total number of visible entries, which excludes items
 * without a label.
 */
static int menu_count_visible(const struct Menu *menu)
{
	struct MenuItem *item;
	int visible_entries = 0;

	for (item = menu->items; (item->label || item->hook); ++item)
	{
		if (!(item->flags & MIF_HIDDEN))
			++visible_entries;
	}

	return visible_entries;
}
#endif

#if CONFIG_MENU_MENUBAR

/*!
 * Update the menu bar according to the selected item
 * and redraw it.
 */
static void menu_update_menubar(
		const struct Menu *menu,
		struct MenuBar *mb,
		int selected)
{
	int item_flags;
#if CPU_HARVARD
	if (menu->flags & MF_ROMITEMS)
	{
		ASSERT(sizeof(menu->items[selected].flags) == sizeof(int));
		item_flags = pgm_read_int(&menu->items[selected].flags);
	}
	else
#endif
		item_flags = menu->items[selected].flags;

	const_iptr_t newlabel = (const_iptr_t)LABEL_OK;

	if (item_flags & MIF_DISABLED)
		newlabel = (const_iptr_t)LABEL_EMPTY;
	else if (item_flags & MIF_TOGGLE)
		newlabel = (const_iptr_t)LABEL_SEL;
	else if (item_flags & MIF_CHECKIT)
	{
		newlabel = (item_flags & MIF_CHECKED) ?
			(const_iptr_t)LABEL_EMPTY : (const_iptr_t)LABEL_SEL;
	}

	mb->labels[3] = newlabel;
	mbar_draw(mb);
}
#endif /* CONFIG_MENU_MENUBAR */


/**
 * Show a menu on the display.
 */
static void menu_layout(
		const struct Menu *menu,
		int first_item,
		int items_per_page,
		int selected)
{
	int ypos, cnt;
	const char * PROGMEM title = PTRMSG(menu->title);

	ypos = 0;

	if (title)
		text_xprintf(menu->bitmap, ypos++, 0, STYLEF_UNDERLINE | STYLEF_BOLD | TEXT_CENTER | TEXT_FILL, title);

#if CONFIG_MENU_SMOOTH
	static coord_t yoffset = 0;
	static int old_first_item = 0;
	if (old_first_item != first_item)
	{
		if (old_first_item > first_item)
		{
			if (++yoffset > menu->bitmap->font->height)
			{
					yoffset = 0;
					--old_first_item;
			}
		}
		else
		{
			if (--yoffset < -menu->bitmap->font->height)
			{
					yoffset = 0;
					++old_first_item;
			}
		}
		first_item = old_first_item;
	}
	text_offset(menu->bitmap, 0, yoffset);
#endif

	for (cnt = 0; cnt < items_per_page; ++cnt)
	{
		const MenuItem *item = &menu->items[first_item + cnt];
#if CPU_HARVARD
		MenuItem ram_item;
		if (menu->flags & MF_ROMITEMS)
		{
			memcpy_P(&ram_item, item, sizeof(ram_item));
			item = &ram_item;
		}
#endif

		/* Check for end of menu */
		if (!(item->label || item->hook))
			break;

		/* Only print visible items */
		if (!(item->flags & MIF_HIDDEN))
		{
#if CPU_HARVARD
			text_xprintf_P
#else
			text_xprintf
#endif
			(
				menu->bitmap, ypos++, 0,
				(first_item + cnt == selected) ? (STYLEF_INVERT | TEXT_FILL) : TEXT_FILL,
				(item->flags & MIF_RAMLABEL) ? PSTR("%s%S") : PSTR("%S%S"),
				PTRMSG(item->label),
				(item->flags & MIF_TOGGLE) ?
					( (item->flags & MIF_CHECKED) ? PSTR(":ON") : PSTR(":OFF") )
					: ( (item->flags & MIF_CHECKED) ? PSTR("\04") : PSTR("") )
			);
		}
	}
}


/*!
 * Handle menu item selection
 */
static void menu_doselect(const struct Menu *menu, struct MenuItem *item)
{
	/* Exclude other items */
	int mask, i;
	for (mask = item->flags & MIF_EXCLUDE_MASK, i = 0; mask; mask >>= 1, ++i)
	{
		if (mask & 1)
			menu->items[i].flags &= ~MIF_CHECKED;
	}

	if (item->flags & MIF_DISABLED)
		return;

	/* Handle checkable items */
	if (item->flags & MIF_TOGGLE)
		item->flags ^= MIF_CHECKED;
	else if (item->flags & MIF_CHECKIT)
		item->flags |= MIF_CHECKED;

	/* Handle items with callback hooks */
	if (item->hook)
	{
		/* Push a jmp buffer to abort the operation with the STOP key */
		if (!PUSH_ABORT)
		{
			item->hook(item->userdata);
			POP_ABORT;
		}
	}
}


/**
 * Return the next visible item (rolls back to the first item)
 */
static int menu_next_visible_item(const struct Menu *menu, int index)
{
	int total = menu_count(menu);
	int item_flags;

	do
	{
		if (++index >= total)
		   index = 0;

#if CPU_HARVARD
		if (menu->flags & MF_ROMITEMS)
		{
			ASSERT(sizeof(menu->items[index].flags) == sizeof(int));
			item_flags = pgm_read_int(&menu->items[index].flags);
		}
		else
#endif
			item_flags = menu->items[index].flags;
	}
	while (item_flags & MIF_HIDDEN);

	return index;
}


/**
 * Return the previous visible item (rolls back to the last item)
 */
static int menu_prev_visible_item(const struct Menu *menu, int index)
{
	int total = menu_count(menu);
	int item_flags;

	do
	{
		if (--index < 0)
			index = total - 1;

#if CPU_HARVARD
		if (menu->flags & MF_ROMITEMS)
		{
			ASSERT(sizeof(menu->items[index].flags) == sizeof(int));
			item_flags = pgm_read_int(&menu->items[index].flags);
		}
		else
#endif
			item_flags = menu->items[index].flags;
	}
	while (item_flags & MIF_HIDDEN);

	return index;
}


/*!
 * Handle a menu and invoke hook functions for the selected menu items.
 */
iptr_t menu_handle(const struct Menu *menu)
{
	uint8_t items_per_page;
	uint8_t first_item, selected;

#if CONFIG_MENU_MENUBAR
	struct MenuBar mb;
	const_iptr_t labels[] =
	{
		(const_iptr_t)LABEL_BACK,
		(const_iptr_t)LABEL_UPARROW,
		(const_iptr_t)LABEL_DOWNARROW,
		(const_iptr_t)0
	};

	/*
	 * Initialize menu bar
	 */
	if (menu->flags & MF_TOPLEVEL)
		labels[0] = (const_iptr_t)LABEL_EMPTY;

	mbar_init(&mb, menu->bitmap, labels, countof(labels));
#endif /* CONFIG_MENU_MENUBAR */


	items_per_page =
		(menu->bitmap->height / menu->bitmap->font->height)
#if CONFIG_MENU_MENUBAR
		- 1 /* menu bar labels */
#endif
		- (menu->title ? 1 : 0);

	/* Selected item should be a visible entry */
	first_item = selected = menu_next_visible_item(menu, menu->selected - 1);

	for(;;)
	{
		keymask_t key;

		/*
		 * Keep selected item visible
		 */
		while (selected < first_item)
			first_item = menu_prev_visible_item(menu, first_item);
		while (selected >= first_item + items_per_page)
			first_item = menu_next_visible_item(menu, first_item);

		/* Clear screen */
		text_clear(menu->bitmap);
		menu_layout(menu, first_item, items_per_page, selected);

		#if CONFIG_MENU_MENUBAR
			menu_update_menubar(menu, &mb, selected);
		#endif

#if CONFIG_MENU_SMOOTH
		key = kbd_peek();
#else
		key = kbd_get();
#endif

		if (key & K_OK)
		{
			struct MenuItem *item = &(menu->items[selected]);
#if CPU_HARVARD
			MenuItem ram_item;
			if (menu->flags & MF_ROMITEMS)
			{
				memcpy_P(&ram_item, item, sizeof(ram_item));
				item = &ram_item;
			}
#endif
			menu_doselect(menu, item);

			/* Return userdata as result */
			if (!menu->flags & MF_STICKY)
			{
				/* Store currently selected item before leaving. */
				if (menu->flags & MF_SAVESEL)
					CONST_CAST(struct Menu *, menu)->selected = selected;
				return item->userdata;
			}
		}
		else if (key & K_UP)
		{
			selected = menu_prev_visible_item(menu, selected);
		}
		else if (key & K_DOWN)
		{
			selected = menu_next_visible_item(menu, selected);
		}
		else if (key & K_CANCEL && !(menu->flags & MF_TOPLEVEL))
		{
			/* Store currently selected item before leaving. */
			if (menu->flags & MF_SAVESEL)
				CONST_CAST(struct Menu *, menu)->selected = selected;
			return 0;
		}
	}
}


/*!
 * Set flags on a menuitem.
 *
 * \param menu  Menu owner of the item to change.
 * \param idx   Index of the menu item.
 * \param flags Bit mask of the flags to set.
 *
 * \return Old flags.
 */
int menu_setFlags(struct Menu *menu, int idx, int flags)
{
	ASSERT(idx < menu_count(menu));
	ASSERT(!(menu->flags & MF_ROMITEMS));

	int old = menu->items[idx].flags;
	menu->items[idx].flags |= flags;
	return old;
}


/*!
 * Clear flags on a menuitem.
 *
 * \param menu  Menu owner of the item to change.
 * \param idx   Index of the menu item.
 * \param flags Bit mask of the flags to clear.
 *
 * \return Old flags.
 */
int menu_clearFlags(struct Menu *menu, int idx, int flags)
{
	ASSERT(idx < menu_count(menu));
	ASSERT(!(menu->flags & MF_ROMITEMS));

	int old = menu->items[idx].flags;
	menu->items[idx].flags &= ~flags;
	return old;
}
