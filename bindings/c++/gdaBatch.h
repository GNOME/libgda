/* GNOME DB libary
 * Copyright (C) 2000 Chris Wiegand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __gda_bindings_cpp_gdaBatchH
#define __gda_bindings_cpp_gdaBatchH

#include "gdaConnection.h"
#include "gdaValue.h"

namespace gda {

class Batch {
	      public:
		Batch ();
        Batch (const Batch& job);
		Batch (GdaBatch* job);
		~Batch ();

        Batch& operator=(const Batch& job);


		bool loadFile (const string& filename, bool clean);
		void addCommand (const string& cmdText);
		void clear ();

		bool start ();
		void stop ();
		bool isRunning ();

		Connection getConnection ();
		void setConnection (const Connection& cnc);
		bool getTransactionMode ();
		void setTransactionMode (bool mode);

	      private:
		GdaBatch* getCStruct (bool refn = true) const;
		void setCStruct (GdaBatch* job);

		void ref () const;
		void unref ();

		GdaBatch* _gda_batch;
		Connection _connection;
	};

};

#endif
