NULL =

public_icons_themes = \
	hicolor	\
	$(NULL)

public_icons = \
	$(NULL)

private_icons = \
	hicolor_actions_16x16_bookmark-view.png \
	hicolor_actions_16x16_history-view.png \
	hicolor_actions_16x16_glade.png \
	hicolor_actions_22x22_bookmark-view.png \
	hicolor_actions_22x22_history-view.png \
	hicolor_actions_22x22_glade.png \
	hicolor_actions_22x22_grid.png \
	hicolor_actions_22x22_form.png \
	hicolor_actions_24x24_bookmark-view.png \
	hicolor_actions_24x24_history-view.png \
	hicolor_actions_24x24_transaction-begin.png \
	hicolor_actions_24x24_transaction-commit.png \
	hicolor_actions_24x24_transaction-rollback.png \
	hicolor_actions_24x24_glade.png \
	hicolor_actions_24x24_table-add.png \
	hicolor_actions_32x32_bookmark-view.png \
	hicolor_actions_32x32_history-view.png \
	hicolor_actions_32x32_glade.png \
	hicolor_actions_32x32_ldap-entries.png \
	hicolor_actions_32x32_table-add.png \
	hicolor_actions_scalable_bookmark-view.svg \
	hicolor_actions_scalable_history-view.svg \
	hicolor_actions_scalable_glade.svg \
	$(NULL)

noinst_DATA = \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		for theme in $(public_icons_themes); do \
			$(gtk_update_icon_cache) $(datadir)/libgda-5.0/icons/$$theme; \
		done; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		for theme in $(public_icons_themes); do \
			echo "***   $(gtk_update_icon_cache) $(datadir)/libgda-5.0/icons/$$theme"; \
		done; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/libgda-5.0/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/libgda-5.0/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/libgda-5.0/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
