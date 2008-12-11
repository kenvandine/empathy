/*
 * Copyright (C) 2008, 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Pierre-Luc Beaudoin <pierre-luc.beaudoin@collabora.co.uk>
 */

#ifndef __EMPATHY_LOCATION_H__
#define __EMPATHY_LOCATION_H__

#include <glib.h>

G_BEGIN_DECLS

/* These keys come from the Telepathy-Spec 0.7.20 */
#define EMPATHY_LOCATION_COUNTRY_CODE "countrycode"
#define EMPATHY_LOCATION_COUNTRY "country"
#define EMPATHY_LOCATION_REGION "region"
#define EMPATHY_LOCATION_LOCALITY "locality"
#define EMPATHY_LOCATION_AREA "area"
#define EMPATHY_LOCATION_POSTAL_CODE "postalcode"
#define EMPATHY_LOCATION_STREET "street"
#define EMPATHY_LOCATION_BUILDING "building"
#define EMPATHY_LOCATION_FLOOR "floor"
#define EMPATHY_LOCATION_ROOM "room"
#define EMPATHY_LOCATION_TEXT "text"
#define EMPATHY_LOCATION_DESCRIPTION "description"
#define EMPATHY_LOCATION_URI "uri"
#define EMPATHY_LOCATION_LAT "lat"
#define EMPATHY_LOCATION_LON "lon"
#define EMPATHY_LOCATION_ALT "alt"
#define EMPATHY_LOCATION_ACCURACY "accuracy"
#define EMPATHY_LOCATION_ACCURACY_LEVEL "accuracy-level"
#define EMPATHY_LOCATION_ERROR "error"
#define EMPATHY_LOCATION_VERTICAL_ERROR_M "vertical-error-m"
#define EMPATHY_LOCATION_HORIZONTAL_ERROR_M "horizontal-error-m"
#define EMPATHY_LOCATION_SPEED "speed"
#define EMPATHY_LOCATION_BEARING "bearing"
#define EMPATHY_LOCATION_CLIMB "climb"
#define EMPATHY_LOCATION_TIMESTAMP "timestamp"

G_END_DECLS

#endif /* __EMPATHY_LOCATION_H__ */
