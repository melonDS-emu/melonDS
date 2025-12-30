#ifndef RHASH_MOCK_FILEREADER_H
#define RHASH_MOCK_FILEREADER_H

#include "rc_export.h"

#include <stdint.h>
#include <stddef.h>

RC_BEGIN_C_DECLS

void init_mock_filereader();
void get_mock_filereader(struct rc_hash_filereader* reader);

#ifndef RC_HASH_NO_DISC
void init_mock_cdreader();
void mock_cd_num_tracks(int num_tracks);
#endif

void rc_hash_reset_filereader();

void mock_file(int index, const char* filename, const uint8_t* buffer, size_t buffer_size);
void mock_file_text(int index, const char* filename, const char* contents);
void mock_empty_file(int index, const char* filename, size_t mock_size);
void mock_file_size(int index, size_t mock_size);
void mock_file_first_sector(int index, int first_sector);

const char* get_mock_filename(void* file_handle);

RC_END_C_DECLS

#endif /* RHASH_MOCK_FILEREADER_H */
