#pragma once
/*
 * storage.h — datos guardados que sobreviven a reinicios Y a flashear firmware nuevo.
 *
 * Regla: cada app usa como `ns` su propio id (el mismo de app_t.id).
 * Una app nunca lee ni borra el ns de otra. El sistema usa "system".
 *
 * Las funciones get devuelven ESP_ERR_NVS_NOT_FOUND si la llave no existe
 * (por ejemplo la primera vez); en ese caso *out no se toca, asi que puedes
 * ponerle antes el valor por defecto.
 * Las llaves miden maximo 15 letras.
 */
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t storage_init(void);   /* lo llama main.c una vez */

esp_err_t storage_get_i32(const char *ns, const char *key, int32_t *out);
esp_err_t storage_set_i32(const char *ns, const char *key, int32_t value);

/* Texto: len = tamano del buffer `out` (incluido el '\0') */
esp_err_t storage_get_str(const char *ns, const char *key, char *out, size_t len);
esp_err_t storage_set_str(const char *ns, const char *key, const char *value);

/* Bloques binarios (structs, listas). *len entra como tamano del buffer y sale como bytes leidos. */
esp_err_t storage_get_blob(const char *ns, const char *key, void *out, size_t *len);
esp_err_t storage_set_blob(const char *ns, const char *key, const void *data, size_t len);

esp_err_t storage_erase_key(const char *ns, const char *key);
esp_err_t storage_erase_all(const char *ns);    /* borra SOLO los datos de ese ns */

#ifdef __cplusplus
}
#endif
