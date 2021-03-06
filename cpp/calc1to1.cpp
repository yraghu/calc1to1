/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

 **************************************************************************/

#include "calc1to1.h"

PREPARE_LOGGING(calc1to1_i)

calc1to1_i::calc1to1_i(const char *uuid, const char *label) :
calc1to1_base(uuid, label)
{
	last_operation = operation;
}

calc1to1_i::~calc1to1_i()
{
}

/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.

    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

	Time:
	    To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();


    Ports:

        Data is passed to the serviceFunction through the getPacket call (BULKIO only).
        The dataTransfer class is a port-specific class, so each port implementing the
        BULKIO interface will have its own type-specific dataTransfer.

        The argument to the getPacket function is a floating point number that specifies
        the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        Each received dataTransfer is owned by serviceFunction and *MUST* be
        explicitly deallocated.

        To send data using a BULKIO interface, a convenience interface has been added 
        that takes a std::vector as the data input

        NOTE: If you have a BULKIO dataSDDS port, you must manually call 
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // this example assumes that the component has two ports:
            //  A provides (input) port of type bulkio::InShortPort called short_in
            //  A uses (output) port of type bulkio::OutFloatPort called float_out
            // The mapping between the port and the class is found
            // in the component base class header file

            bulkio::InShortPort::dataTransfer *tmp = short_in->getPacket(bulkio::Const::BLOCKING);
            if (not tmp) { // No data is available
                return NOOP;
            }

            std::vector<float> outputData;
            outputData.resize(tmp->dataBuffer.size());
            for (unsigned int i=0; i<tmp->dataBuffer.size(); i++) {
                outputData[i] = (float)tmp->dataBuffer[i];
            }

            // NOTE: You must make at least one valid pushSRI call
            if (tmp->sriChanged) {
                float_out->pushSRI(tmp->SRI);
            }
            float_out->pushPacket(outputData, tmp->T, tmp->EOS, tmp->streamID);

            delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the std::vector passed from/to BulkIO can be typecast to/from
        std::vector< std::complex<dataType> >.  For example, for short data:

            bulkio::InShortPort::dataTransfer *tmp = myInput->getPacket(bulkio::Const::BLOCKING);
            std::vector<std::complex<short> >* intermediate = (std::vector<std::complex<short> >*) &(tmp->dataBuffer);
            // do work here
            std::vector<short>* output = (std::vector<short>*) intermediate;
            myOutput->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

        Interactions with non-BULKIO ports are left up to the component developer's discretion

    Properties:

        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given a generated name of the form
        "prop_n", where "n" is the ordinal number of the property in the PRF file.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (calc1to1_base).

        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput

            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }

        A callback method can be associated with a property so that the method is
        called each time the property value changes.  This is done by calling 
 setPropertyChangeListener(<property name>, this, &calc1to1_i::<callback method>)
 in the constructor.

 Example:
 // This example makes use of the following Properties:
 //  - A float value called scaleValue

 //Add to calc1to1.cpp
 calc1to1_i::calc1to1_i(const char *uuid, const char *label) :
 calc1to1_base(uuid, label)
 {
 setPropertyChangeListener("scaleValue", this, &calc1to1_i::scaleChanged);
 }

 void calc1to1_i::scaleChanged(const std::string& id){
 std::cout << "scaleChanged scaleValue " << scaleValue << std::endl;
 }

 //Add to calc1to1.h
 void scaleChanged(const std::string&);


 ************************************************************************************************/
int calc1to1_i::serviceFunction() {
	LOG_DEBUG(calc1to1_i, "serviceFunction() example log message");
	bulkio::InDoublePort::dataTransfer *tmp = input_double->getPacket(
			bulkio::Const::BLOCKING);
	if (not tmp) { // No data is available
		return NOOP;
	}

	if (last_operation != operation)
		data.clear();

	std::vector<double> *output = NULL;

	if (tmp->SRI.mode) {
		// input is complex, operand can be real/complex
		vector<complex<double> > *input = (vector<complex<double> >*) &(tmp->dataBuffer);
		doCalculation(*input, operand, data);
		output = &data;
	} else if (operand.imag() != 0) {
		// input is real, operand is complex
		tmp->sriChanged = true;
		tmp->SRI.mode = true;
		vector<double> *input = (vector<double>*) &tmp->dataBuffer;
		doCalculation(*input, operand, data);
		output = &data;
	} else {
		// both are real
		vector<double> *input = (vector<double>*) &tmp->dataBuffer;
		doCalculation(*input, operand.real(), data);
		output = &data;
	}

	if (tmp->sriChanged) {
		output_double->pushSRI(tmp->SRI);
	}
	output_double->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

	delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
	return NORMAL;
}
