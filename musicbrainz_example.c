/*

    Copyright (C) 2012 Andrew Hawkins
    Copyright (C) 2020 Andreas Rönnquist <andreas@ronnquist.net>

    Any code in this file may be redistributed or modified under the terms of
    the GNU General Public Licence as published by the Free Software
    Foundation; version 2 of the licence.

    Based on MusicBrainz example program
    https://github.com/metabrainz/libmusicbrainz/blob/master/examples/cdlookup_c.c

*/

/*

    Buildable with a command line like this:

    gcc -g -o musicbrainz_example musicbrainz_example.c `pkg-config libmusicbrainz5 --cflags --libs` `pkg-config libdiscid --cflags --libs` `pkg-config glib-2.0 --cflags --libs`

    To build on a Debian machine, requires the libraries libdiscid-dev, libmusicbrainz5-dev and libglib2.0-dev
    (available in the Debian repositories).

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "musicbrainz5/mb5_c.h"

#include "discid/discid.h"

gchar *time_string(int value)
{
    int h, m, s;
    gchar *result = NULL;

    h = (value / 3600);
    m = (value / 60) % 60;
    s = value % 60;

    result = g_strdup_printf("%02d:%02d:%02d", h, m, s);

    return result;
}

    
int cd_lookup(char *discid)
{
    Mb5ArtistCredit artist_credit;
    Mb5NameCreditList name_credit_list;
    int required_size;
    
    Mb5Query query = mb5_query_new("musicbrainz_example-1.0", NULL, 0);
    if (query)
    {
        Mb5Metadata metadata1 = mb5_query_query(query, "discid", discid, "", 0, NULL, NULL);
        char errormessage[256];

        tQueryResult result = mb5_query_get_lastresult(query);
        int httpcode = mb5_query_get_lasthttpcode(query);

        mb5_query_get_lasterrormessage(query, errormessage, sizeof(errormessage));
        printf("Result: %d\nHTTPCode: %d\nErrorMessage: '%s'\n", result, httpcode, errormessage);

        if (metadata1)
        {
            Mb5Disc disc = mb5_metadata_get_disc(metadata1);
            if (disc)
            {
                Mb5ReleaseList release_list = mb5_disc_get_releaselist(disc);
                if (release_list)
                {
                    /*
                     *if we want to keep an object around for a while, we can
                     *clone it. We are now responsible for deleting the object
                    */

                    int current_release = 0;

                    printf("Found %d release(s)\n", mb5_release_list_size(release_list));
                    
                    printf("---------------------------------\n");

                    for (current_release = 0; current_release < mb5_release_list_size(release_list); current_release++)
                    {
                        Mb5Metadata metadata2 = 0;
                        Mb5Release Release = mb5_release_list_item(release_list, current_release);

                        if (Release)
                        {
                            /* The releases returned from LookupDiscID don't contain full information */

                            char **ParamNames;
                            char **ParamValues;
                            char release_ID[256];

                            ParamNames = malloc(sizeof(char *));
                            ParamNames[0] = malloc(256);
                            ParamValues = malloc(sizeof(char *));
                            ParamValues[0] = malloc(256);

                            strcpy(ParamNames[0], "inc");
                            strcpy(ParamValues[0], "artists labels recordings release-groups url-rels discids artist-credits");

                            mb5_release_get_id(Release, release_ID, sizeof(release_ID));

                            metadata2 = mb5_query_query(query, "release", release_ID, "", 1, ParamNames, ParamValues);

                            if (metadata2)
                            {
                                Mb5Release full_release = mb5_metadata_get_release(metadata2);
                                
                                
                                // Get the album artist
                                artist_credit = mb5_release_get_artistcredit(full_release);
                                name_credit_list = mb5_artistcredit_get_namecreditlist(artist_credit);
                                
                                for (int i = 0; i < mb5_namecredit_list_size (name_credit_list); i++) {
                                    Mb5NameCredit  name_credit = mb5_namecredit_list_item (name_credit_list, i);
                                    Mb5Artist      artist;
                                    char          *artist_name = NULL;
                                    // char          *artist_id = NULL;

                                    artist = mb5_namecredit_get_artist (name_credit);

                                    required_size = mb5_artist_get_name (artist, artist_name, 0);
                                    artist_name = g_new (char, required_size + 1);
                                    mb5_artist_get_name (artist, artist_name, required_size + 1);
                                    //debug (DEBUG_INFO, "==> [MB] ARTIST NAME: %s\n", artist_name);
                                    printf("Release artist: %s\n", artist_name);

                                    g_free (artist_name);
                                }
                                
                                if (full_release)
                                {
                                    /*
                                     * However, these releases will include information for all media in the release
                                     * So we need to filter out the only the media we want.
                                     */

                                    Mb5MediumList MediumList = mb5_release_media_matching_discid(full_release, discid);
                                    if (MediumList)
                                    {
                                        if (mb5_medium_list_size(MediumList))
                                        {
                                            int current_medium = 0;

                                            Mb5ReleaseGroup ReleaseGroup = mb5_release_get_releasegroup(full_release);
                                            if (ReleaseGroup)
                                            {
                                                /* One way of getting a string, just use a buffer that
                                                 * you're pretty sure will accomodate the whole string
                                                 */

                                                char Title[256];

                                                mb5_releasegroup_get_title(ReleaseGroup, Title, sizeof(Title));
                                                printf("Release group title: '%s'\n", Title);
                                            }
                                            else
                                                printf("No release group for this release\n");

                                            printf("Found %d media item(s)\n", mb5_medium_list_size(MediumList));

                                            for (current_medium = 0; current_medium < mb5_medium_list_size(MediumList); current_medium++)
                                            {
                                                Mb5Medium Medium = mb5_medium_list_item(MediumList, current_medium);
                                                if (Medium)
                                                {
                                                    int alloc_size = 10;
                                                    char *MediumTitle = malloc(alloc_size);
                                                    int required_size;

                                                    Mb5TrackList TrackList = mb5_medium_get_tracklist(Medium);

                                                    /* Another way of getting a string. Preallocate a buffer
                                                     * and check if if was big enough when retrieving string.
                                                     * If not, reallocate it to be big enough and get it again.
                                                     */

                                                    required_size = mb5_medium_get_title(Medium, MediumTitle, alloc_size);
                                                    if (required_size > alloc_size)
                                                    {
                                                        MediumTitle = realloc(MediumTitle, required_size+1);
                                                        mb5_medium_get_title(Medium, MediumTitle, required_size+1);
                                                    }

                                                    printf("Found media: '%s', position %d\n", MediumTitle, mb5_medium_get_position(Medium));
                                                    
                                                    GSList *artist_list = NULL;
                                                    GSList *temp_list = NULL;
                                                    
                                                    gchar *temp_string = NULL;
                                                    
                                                    gboolean compilation = FALSE;
                                                           
                                                    if (TrackList)
                                                    {
                                                        int current_track=0;
                                                        
                                                        
                                                        gchar *temp_artist = NULL; // g_strdup("");
                                                        
                                                        temp_list = artist_list;
                                                            

                                                        for (current_track = 0; current_track < mb5_track_list_size(TrackList); current_track++)
                                                        {
                                                            Mb5Track track=mb5_track_list_item(TrackList, current_track);
                                                            Mb5Recording recording=mb5_track_get_recording(track);
                                                            
                                                            
                                                            // Get the artist from track
                                                            if (recording)
                                                            {
                                                                Mb5ArtistCredit track_artist_credit = mb5_artistcredit_clone(mb5_recording_get_artistcredit(recording));
                                                                Mb5NameCreditList temp_name_credit_list = mb5_artistcredit_get_namecreditlist(track_artist_credit);
                                                                
                                                                Mb5NameCreditList new_name_credit_list = mb5_namecredit_list_clone(temp_name_credit_list);
                                                                
                                                                // Get per track artist
                                                                
                                                                int size = mb5_namecredit_list_size(new_name_credit_list);
                                
                                                                for (int i = 0; i < size; i++) {
                                                                    Mb5NameCredit new_name_credit = mb5_namecredit_list_item (new_name_credit_list, i);
                                                                    Mb5Artist artist;
                                                                    char *artist_name = NULL;
                                                                    
                                                                    artist = mb5_namecredit_get_artist (new_name_credit);

                                                                    required_size = mb5_artist_get_name (artist, artist_name, 0);
                                                                    artist_name = g_new (char, required_size + 1);
                                                                    mb5_artist_get_name (artist, artist_name, required_size + 1);

                                                                    gchar *temp_artist = g_strdup(artist_name);
                                                                    
                                                                    g_free(artist_name);

                                                                    temp_list = g_slist_append(temp_list, temp_artist);
                                                                }
                                                                
                                                                artist_list = temp_list;
                                                                
                                                                mb5_artistcredit_delete(track_artist_credit);
                                                                
                                                                mb5_namecredit_list_delete(new_name_credit_list);
                                                            }
                                                        }
                                                        
                                                        // Check if it is a Compilation CD (This might be doable using the MusicBrainz API,
                                                        // I haven't checked carefully. I assume it is, if there's several different artists, and
                                                        // not one and the same for all the tracks.
                                                        //
                                                        if (artist_list != NULL) {
                                                            
                                                            for (temp_list = artist_list; temp_list != NULL; temp_list = temp_list -> next) {
                                                                
                                                                if (temp_string == NULL) {
                                                                    temp_string = g_strdup(temp_list->data);
                                                                } else {
                                                                    
                                                                    if (g_strcmp0(temp_list->data, temp_string) != 0) {
                                                                        compilation = TRUE;
                                                                    }
                                                                }
                                                            }
                                                            if (temp_string != NULL) {
                                                                g_free(temp_string);
                                                            }
                                                        }
                                                    }

                                                    if (TrackList)
                                                    {
                                                        int current_track = 0;
                                                        
                                                        GSList *list = artist_list;

                                                        int tracklist_offset = mb5_track_list_get_offset(TrackList);

                                                        printf("Tracklist offset: %d\n", tracklist_offset);

                                                        for (current_track = 0; current_track < mb5_track_list_size(TrackList); current_track++)
                                                        {
                                                            char *track_title = 0;
                                                            int required_length = 0;

                                                            Mb5Track track = mb5_track_list_item(TrackList, current_track);
                                                            Mb5Recording recording = mb5_track_get_recording(track);
                                                            
                                                            /* Yet another way of getting string. Call it once to
                                                             * find out how long the buffer needs to be, allocate
                                                             * enough space and then call again.
                                                             */
                                                            
                                                            if (recording)
                                                            {
                                                                required_length = mb5_recording_get_title(recording, track_title, 0);
                                                                track_title = malloc(required_length + 1);
                                                                mb5_recording_get_title(recording, track_title, required_length + 1);
                                                            }
                                                            else
                                                            {
                                                                required_length = mb5_track_get_title(track, track_title, 0);
                                                                track_title = malloc(required_length + 1);
                                                                mb5_track_get_title(track, track_title, required_length + 1);
                                                            }

                                                            int track_length = mb5_track_get_length(track);

                                                            gchar *temp_time_string = time_string(track_length / 1000);

                                                            // If a compilation, print artist for each track.
                                                            if (compilation) {
                                                                printf("%02d - %s - '%s' (%s)\n", mb5_track_get_position(track), (gchar*)(list->data), track_title, temp_time_string);
                                                            } else {
                                                                printf("%02d - '%s' (%s)\n", mb5_track_get_position(track), track_title, temp_time_string);
                                                            }
                                                            
                                                            g_free(temp_time_string);

                                                            list = list -> next;

                                                            free(track_title);
                                                        }
                                                    }
                                                    
                                                    printf("Compilation: ");
                                                    
                                                    if (compilation) {
                                                        printf("Yes");
                                                    } else {
                                                        printf("No");
                                                    }
                                                    
                                                    printf("\n");
                                                    
                                                    g_slist_free_full(artist_list, g_free);

                                                    free(MediumTitle);
                                                }
                                            }
                                        }

                                        /* We must delete the result of 'media_matching_discid' */
                                        mb5_medium_list_delete(MediumList);
                                    }
                                }

                                /* We must delete anything returned from the query methods */
                                mb5_metadata_delete(metadata2);
                            }

                            free(ParamValues[0]);
                            free(ParamValues);
                            free(ParamNames[0]);
                            free(ParamNames);
                        }
                    }

                    /* We must delete anything we have cloned */
                    // mb5_release_list_delete(CloneReleaseList);
                }
            }

            /* We must delete anything returned from the query methods */
            mb5_metadata_delete(metadata1);
        }

        /* We must delete the original query object */
        mb5_query_delete(query);
    }
    
    return 0;
}


int main(int argc, const char *argv[])
{
    DiscId *disc = discid_new();
    
    if ( discid_read_sparse(disc, "/dev/cdrom", 0) == 0 ) {
        fprintf(stderr, "Error: %s\n", discid_get_error_msg(disc));

        discid_free(disc);
        return 1;
    }

    char *discid = discid_get_id(disc);

    printf("DiscID: %s\n", discid);

    cd_lookup(discid);

    discid_free(disc);

    return 0;
}
