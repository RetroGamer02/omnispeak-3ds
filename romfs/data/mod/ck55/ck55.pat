%ext ck5                                            # Keen 5.5 - Release Five
%version 1.4                                        # by The CK Guy
%maphead maphead.ck5                                # Replace maphead resource
%egahead egahead.ck5                                # Replace egahead resource
%patchfile $25b22 tileinfo.ck5                      # Replace tile properties
%level.name 0 Omegamatic II                         # Replace level names
%level.name 1 Exhaust Port 314
%level.name 2 MS Tech Support
%level.name 3 Layer Zero Defense
%level.name 4 Power Supply Center
%level.name 5 L-337
%level.name 6 Not So Fast
%level.name 7 The Atrium
%level.name 8 Double Toggle
%level.name 9 The Shelley Shack
%level.name 10 Ghost Town
%level.name 11 Celeritas
%level.name 12 theInfernalM`~_|
%level.name 13 Quick and Easy
%level.name 14 The Omega Guard
%level.entry 0 Keen feels he\nis unwelcome          # Replace level entry texts
%level.entry 1 Exhaust Port 314\nUncanny, isn't it?
%level.entry 2 Keen is attacked\nby Microsoft\nTech Support
%level.entry 3 Keen peels back\nLayer Zero Defense
%level.entry 4 Keen investigates the\nPower Supply Center
%level.entry 5 \nIt's not what you say,\n17'5 h0w y0u 54y 17z0rz
%level.entry 6 So close,\nand yet\nso far
%level.entry 7 Keen takes a\nbreath of fresh air
%level.entry 8 Keen is pulled into\nDouble Toggle
%level.entry 9 Keen drops into\nThe Shelley Shack
%level.entry 10 Is there anybody\nin there ... ?
%level.entry 11 Keen speeds into\nCeleritas
%level.entry 12 Last stop:\ntheInfernalM`~_|
%level.entry 13 This shouldn't\ntake long ...
%level.entry 14 Keen finally reaches\nThe Omega Guard
%patchfile $20cd8 startup.ck5                       # Replace start-up screen
%patchfile $1fde0 story.ck5                         # Replace "Star Wars" text
%patch $b97d $eb                                    # Easy -> Same Jump Height
%patch $305e3 "Want to try again on" $00            # Miscellaneous text
%patch $305fb "Of Course"
%patch $30605 "Bah - Not Now" $00
%patch $31bfb "\nFuses?\nWhat fuses?" $00
%patch $31c20 "\nFuses?\nWhat fuses?" $00
%patch $33f16 "The CK Guy"
#%patch $77ee $eb $6d                                # Disable B+A+T cheat
#%patch $7a79 $eb                                    # Disable F10+[...]
#%patch $8260 $eb                                    # Disable A+2+[Enter]
%end
