/* Copyright 2015 University of Szeged.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "js-parser-defines.h"

int main (int argc, char* argv[])
{
  uint8_t *input;
  FILE *input_file;
  long int input_size;

  if (argc <= 1)
  {
    fprintf (stderr, "File name required!\n");
    return -1;
  }

  input_file = fopen (argv[1], "rb");
  if (!input_file)
  {
    fprintf (stderr, "Cannot open '%s'\n", argv[1]);
    return -1;
  }

  fseek (input_file, 0, SEEK_END);
  input_size = ftell (input_file);
  fseek (input_file, 0, SEEK_SET);

  input = (uint8_t *) malloc (input_size);
  if (!input)
  {
    fclose (input_file);
    fprintf (stderr, "Cannot allocate memory\n");
    return -1;
  }

  if (fread (input, 1, input_size, input_file) == input_size)
  {
    fprintf (stderr, "File '%s' is loaded (size: %ld bytes)\n", argv[1], input_size);
    parser_parse_script (input, (size_t) input_size);
  }
  else
  {
    fprintf (stderr, "Cannot read '%s'\n", argv[1]);
  }

  free (input);
  fclose (input_file);

  return 0;
} /* main */
