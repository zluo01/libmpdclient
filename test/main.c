/* libmpdclient
   (c) 2003-2009 The Music Player Daemon Project
   This project's homepage is: http://www.musicpd.org

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Music Player Daemon nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#include <mpd/client.h>
#include <mpd/status.h>
#include <mpd/song.h>
#include <mpd/entity.h>
#include <mpd/search.h>
#include <mpd/tag.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define BRIGHT 1
#define RED 31
#define GREEN 32
#define YELLOW 33
#define BG_BLACK 40
#define COLOR_CODE 0x1B

#define LOG_INFO(x, ...) {printf("    [info]" x "\n", __VA_ARGS__);}
#define LOG_WARNING(x, ...) \
{\
	fprintf(stderr, "%c[%d;%d;%dm[WARNING](%s:%d) %s : " x "\n", COLOR_CODE, BRIGHT, YELLOW, BG_BLACK, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);\
	printf("%c[%dm", 0x1B, 0);\
}

#define LOG_ERROR(x, ...) \
{\
	fprintf(stderr, "%c[%d;%d;%dm[ERROR](%s:%d) %s : " x "\n", COLOR_CODE, BRIGHT, RED, BG_BLACK, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);\
	printf("%c[%dm", 0x1B, 0);\
}

#define START_TEST(description, method, ...) \
{\
	printf("[Start Test] " description "\n");\
	if (method(__VA_ARGS__) < 0)\
		printf("%c[%d;%d;%dm[End Test: ERROR]\n", COLOR_CODE, BRIGHT, RED, BG_BLACK);\
	else\
		printf("%c[%d;%d;%dm[End Test: OK]\n", COLOR_CODE, BRIGHT, GREEN, BG_BLACK);\
	printf("%c[%dm", 0x1B, 0);\
}

#define CHECK_CONNECTION(conn) \
	if (mpd_get_error(conn) != MPD_ERROR_SUCCESS) { \
		LOG_ERROR("%s", mpd_get_error_message(conn)); \
		return -1; \
	}

static int
test_new_connection(struct mpd_connection **conn)
{
	const char *hostname = getenv("MPD_HOST");
	const char *port = getenv("MPD_PORT");

	if (hostname == NULL)
		hostname = "localhost";
	if (port == NULL)
		port = "6600";

	*conn = mpd_connection_new(hostname, atoi(port), 10);

	if (!*conn || mpd_get_error(*conn) != MPD_ERROR_SUCCESS) {
		LOG_ERROR("%s", mpd_get_error_message(*conn));
		mpd_connection_free(*conn);
		*conn = NULL;
		return -1;
	}
	return 0;
}

static int
test_version(struct mpd_connection *conn)
{
	int i, total = -1;
	for (i=0; i<3; ++i) {
		LOG_INFO("version[%i]: %i", i, mpd_get_server_version(conn)[i]);
		total += mpd_get_server_version(conn)[i];
	}
	/* Check if at least one of the three number is positive */
	return total;
}

static void
print_status(struct mpd_status *status)
{
	LOG_INFO("volume: %i", mpd_status_get_volume(status));
	LOG_INFO("repeat: %i", mpd_status_get_repeat(status));
	LOG_INFO("single: %i", mpd_status_get_single(status));
	LOG_INFO("consume: %i", mpd_status_get_consume(status));
	LOG_INFO("random: %i", mpd_status_get_random(status));
	LOG_INFO("playlist: %u", mpd_status_get_playlist_version(status));
	LOG_INFO("playlistLength: %i", mpd_status_get_playlist_length(status));

	if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	    mpd_status_get_state(status) == MPD_STATE_PAUSE) {
		LOG_INFO("song: %i", mpd_status_get_song_pos(status));
		LOG_INFO("elaspedTime: %i", mpd_status_get_elapsed_time(status));
		LOG_INFO("totalTime: %i", mpd_status_get_total_time(status));
		LOG_INFO("bitRate: %i", mpd_status_get_bit_rate(status));
		LOG_INFO("sampleRate: %i", mpd_status_get_sample_rate(status));
		LOG_INFO("bits: %i", mpd_status_get_bits(status));
		LOG_INFO("channels: %i", mpd_status_get_channels(status));
	}
}

static void
print_tag(const struct mpd_song *song, enum mpd_tag_type type,
	  const char *label)
{
	unsigned i = 0;
	const char *value;

	while ((value = mpd_song_get_tag(song, type, i++)) != NULL)
		LOG_INFO("%s: %s", label, value);
}

static void
print_song(const struct mpd_song *song)
{
	print_tag(song, MPD_TAG_FILENAME, "file");
	print_tag(song, MPD_TAG_ARTIST, "artist");
	print_tag(song, MPD_TAG_ALBUM, "album");
	print_tag(song, MPD_TAG_TITLE, "title");
	print_tag(song, MPD_TAG_TRACK, "track");
	print_tag(song, MPD_TAG_NAME, "name");
	print_tag(song, MPD_TAG_DATE, "date");

	if (mpd_song_get_time(song) != MPD_SONG_NO_TIME)
		LOG_INFO("time: %i", mpd_song_get_time(song));

	if (mpd_song_get_pos(song) != MPD_SONG_NO_NUM)
		LOG_INFO("pos: %i", mpd_song_get_pos(song));
}

static int
test_status(struct mpd_connection *conn)
{
	struct mpd_status *status;

	status = mpd_run_status(conn);
	if (!status) {
		LOG_ERROR("%s", mpd_get_error_message(conn));
		return -1;
	}

	print_status(status);
	mpd_status_free(status);

	mpd_response_finish(conn);
	CHECK_CONNECTION(conn);

	return 0;
}

static int
test_currentsong(struct mpd_connection *conn)
{
	struct mpd_song *song;

	mpd_send_currentsong(conn);

	CHECK_CONNECTION(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
		print_song(song);

		mpd_song_free(song);
	}

	mpd_response_finish(conn);
	CHECK_CONNECTION(conn);

	return 0;
}


static int
test_list_status_currentsong(struct mpd_connection *conn)
{
	struct mpd_status *status;
	const struct mpd_song *song;
	struct mpd_entity *entity;

	CHECK_CONNECTION(conn);

	mpd_command_list_begin(conn, true);
	mpd_send_status(conn);
	mpd_send_currentsong(conn);
	mpd_command_list_end(conn);

	CHECK_CONNECTION(conn);

	status = mpd_recv_status(conn);
	if (!status) {
		LOG_ERROR("%s", mpd_get_error_message(conn));
		return -1;
	}
	if (mpd_status_get_error(status)) {
		LOG_WARNING("status error: %s", mpd_status_get_error(status));
	}

	print_status(status);
	mpd_status_free(status);

	CHECK_CONNECTION(conn);

	mpd_response_next(conn);

	entity = mpd_recv_entity(conn);
	if (entity) {
		if (mpd_entity_get_type(entity) != MPD_ENTITY_TYPE_SONG) {
			LOG_ERROR("entity doesn't have the expected type (song)i :%d",
				  mpd_entity_get_type(entity));
			mpd_entity_free(entity);
			return -1;
		}

		song = mpd_entity_get_song(entity);

		print_song(song);

		mpd_entity_free(entity);
	}

	mpd_response_finish(conn);
	CHECK_CONNECTION(conn);

	return 0;
}

static int
test_lsinfo(struct mpd_connection *conn, const char *path)
{
	struct mpd_entity *entity;

	mpd_send_lsinfo(conn, path);
	CHECK_CONNECTION(conn);

	while ((entity = mpd_recv_entity(conn)) != NULL) {
		const struct mpd_song *song;
		const struct mpd_directory *dir;
		const struct mpd_playlist *pl;

		switch (mpd_entity_get_type(entity)) {
		case MPD_ENTITY_TYPE_UNKNOWN:
			printf("Unknown type\n");
			break;

		case MPD_ENTITY_TYPE_SONG:
			song = mpd_entity_get_song(entity);
			print_song (song);
			break;

		case MPD_ENTITY_TYPE_DIRECTORY:
			dir = mpd_entity_get_directory(entity);
			printf("directory: %s\n", mpd_directory_get_path(dir));
			break;

		case MPD_ENTITY_TYPE_PLAYLIST:
			pl = mpd_entity_get_playlist(entity);
			LOG_INFO("playlist: %s", mpd_playlist_get_path(pl));
			break;
		}

		mpd_entity_free(entity);
	}

	mpd_response_finish(conn);
	CHECK_CONNECTION(conn);

	return 0;
}

static int
test_list_artists(struct mpd_connection *conn)
{
	char *artist;
	int first = 1;

        mpd_search_db_tags(conn, MPD_TAG_ARTIST);
	mpd_search_commit(conn);
	CHECK_CONNECTION(conn);

	LOG_INFO("%s: ", "Artists list");
	while ((artist = mpd_get_next_tag(conn, MPD_TAG_ARTIST))) {
		if (first) {
			printf("    %s", artist);
			first = 0;
		} else {
			printf(", %s", artist);
		}
		free(artist);
	}
	printf("\n");

	mpd_response_finish(conn);
	CHECK_CONNECTION(conn);

	return 0;
}

static int
test_close_connection(struct mpd_connection *conn)
{
	mpd_connection_free(conn);
	return 0;
}

int
main(int argc, char ** argv)
{
	struct mpd_connection *conn = NULL;
	const char *lsinfo_path = "/";

	if (argc==2)
		lsinfo_path = argv[1];

	START_TEST("Test connection to MPD server", test_new_connection, &conn);
	if (!conn)
		return -1;

	START_TEST("Check MPD versions", test_version, conn);
	START_TEST("'status' command", test_status, conn);
	START_TEST("'currentsong' command", test_currentsong, conn);
	START_TEST("List commands: 'status' and 'currentsong'", test_list_status_currentsong, conn);
	START_TEST("'lsinfo' command", test_lsinfo, conn, lsinfo_path);
	START_TEST("'list artist' command", test_list_artists, conn);
	START_TEST("Test connection closing", test_close_connection, conn);

	return 0;
}
