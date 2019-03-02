/******************************************************************************
 **                           COPYRIGHT NOTICE
 **      (c) 2012 The Johns Hopkins University Applied Physics Laboratory
 **                         All rights reserved.
 ******************************************************************************/

/*****************************************************************************
 **
 ** \file pdu.c
 **
 **
 ** Description:
 **
 ** Notes:
 **
 ** Assumptions:
 **
 ** Modification History:
 **  MM/DD/YY  AUTHOR         DESCRIPTION
 **  --------  ------------   ---------------------------------------------
 **  09/24/12  E. Birrane     Initial Implementation (JHU/APL)
 **  11/01/12  E. Birrane     Redesign of messaging architecture. (JHU/APL)
 **  06/25/13  E. Birrane     Renamed message "bundle" message "group". (JHU/APL)
 **  06/26/13  E. Birrane     Added group timestamp (JHU/APL)
 **  10/01/18  E. Birrane     Updated to AMP v0.5. Migrate from pdu.h. (JHU/APL)
 *****************************************************************************/

#include "platform.h"
#include "ion.h"

#include "../adm/adm.h"
#include "../msg/msg.h"
#include "../utils/utils.h"
#include "../utils/vector.h"



msg_hdr_t msg_hdr_deserialize(CborValue *it, int *success)
{
	msg_hdr_t hdr;

	*success = cut_get_cbor_numeric(it, AMP_TYPE_BYTE, &(hdr.flags));

	return hdr;
}

CborError msg_hdr_serialize(CborEncoder *encoder, msg_hdr_t hdr)
{
	return cut_enc_byte(encoder, hdr.flags);
}



msg_agent_t* msg_agent_create()
{
	msg_agent_t *result = STAKE(sizeof(msg_agent_t));
	CHKNULL(result);

	MSG_HDR_SET_OPCODE(result->hdr.flags,MSG_TYPE_REG_AGENT);

	return result;
}


msg_agent_t *msg_agent_deserialize(blob_t *data, int *success)
{
	msg_agent_t *result = msg_agent_create();
	CborError err;
	size_t len;
	CborParser parser;
	CborValue it;
	eid_t tmp;

	*success = AMP_FAIL;
	CHKNULL(data);
	CHKNULL(result);

	if(cbor_parser_init(data->value, data->length, 0, &parser, &it) != CborNoError)
	{
		return NULL;
	}

	/* Step 1: Grab the header. */
	result->hdr = msg_hdr_deserialize(&it, success);
	if(*success != AMP_OK)
	{
		msg_agent_release(result, 1);
		return NULL;
	}

	len = AMP_MAX_EID_LEN;
	err = cbor_value_copy_text_string(&it, tmp.name, &len, &it);
	msg_agent_set_agent(result, tmp);

	if(err != CborNoError)
	{
		AMP_DEBUG_ERR("msg_agent_deserialize", "can't copy text string: %d", err);
		*success = AMP_FAIL;
		msg_agent_release(result, 1);
		return NULL;
	}

	return result;
}



void msg_agent_release(msg_agent_t *msg, int destroy)
{
	CHKVOID(msg);

	if(destroy)
	{
		SRELEASE(msg);
	}
}



CborError msg_agent_serialize(CborEncoder *encoder, void *item)
{
	CborError err;
	msg_agent_t *msg = (msg_agent_t *) item;

	err = msg_hdr_serialize(encoder, msg->hdr);
	if(((err != CborNoError) && (err != CborErrorOutOfMemory)))
	{
		return err;
	}

	err = cbor_encode_text_stringz(encoder, msg->agent_id.name);

	return err;
}



blob_t*   msg_agent_serialize_wrapper(msg_agent_t *msg)
{
	return cut_serialize_wrapper(MSG_DEFAULT_ENC_SIZE, msg, msg_agent_serialize);
}


void msg_agent_set_agent(msg_agent_t *msg, eid_t agent)
{
	CHKVOID(msg);
	msg->agent_id = agent;
}

msg_ctrl_t* msg_ctrl_create()
{
	msg_ctrl_t *result = STAKE(sizeof(msg_ctrl_t));
	CHKNULL(result);

	MSG_HDR_SET_OPCODE(result->hdr.flags,MSG_TYPE_PERF_CTRL);

	return result;
}

msg_ctrl_t* msg_ctrl_create_ari(ari_t *id)
{
	msg_ctrl_t *msg = NULL;

	CHKNULL(id);
	msg = msg_ctrl_create();
	CHKNULL(msg);
	if((msg->ac = ac_create()) == NULL)
	{
		msg_ctrl_release(msg, 1);
		return NULL;
	}

	if(vec_push(&(msg->ac->values), id) != VEC_OK)
	{
		msg_ctrl_release(msg, 1);
		return NULL;
	}

	return msg;
}

msg_ctrl_t* msg_ctrl_deserialize(blob_t *data, int *success)
{
	msg_ctrl_t *result = msg_ctrl_create();
	CborParser parser;
	CborValue it;

	*success = AMP_FAIL;
	CHKNULL(data);
	CHKNULL(result);

	if(cbor_parser_init(data->value, data->length, 0, &parser, &it) != CborNoError)
	{
		return NULL;
	}

	/* Step 1: Grab the header. */
	result->hdr = msg_hdr_deserialize(&it, success);
	if(*success != AMP_OK)
	{
		msg_ctrl_release(result, 1);
		return NULL;
	}

	/* Step 2: Grab the start time. */
	*success = cut_get_cbor_numeric(&it, AMP_TYPE_UVAST, &(result->start));
	if(*success != AMP_OK)
	{
		msg_ctrl_release(result, 1);
		return NULL;
	}

	/* Step 3: Grab the macro. */
	cut_enc_refresh(&it);

	result->ac = ac_deserialize_ptr(&it, success);
	if(*success != AMP_OK)
	{
		msg_ctrl_release(result, 1);
		return NULL;
	}

	return result;
}


void msg_ctrl_release(msg_ctrl_t *msg, int destroy)
{
	CHKVOID(msg);
	ac_release(msg->ac, 1);

	if(destroy)
	{
		SRELEASE(msg);
	}
}


CborError msg_ctrl_serialize(CborEncoder *encoder, void *item)
{
	CborError err;
	msg_ctrl_t *msg = (msg_ctrl_t*) item;

	err = msg_hdr_serialize(encoder, msg->hdr);
	if(((err != CborNoError) && (err != CborErrorOutOfMemory)))
	{
		return err;
	}


	err = cbor_encode_uint(encoder, msg->start);
	if(((err != CborNoError) && (err != CborErrorOutOfMemory)))
	{
		return err;
	}


	err = ac_serialize(encoder, msg->ac);

	return err;
}



blob_t* msg_ctrl_serialize_wrapper(msg_ctrl_t *msg)
{
	return cut_serialize_wrapper(MSG_DEFAULT_ENC_SIZE, msg, msg_ctrl_serialize);
}



// Shallow copy.
int msg_rpt_add_rpt(msg_rpt_t *msg, rpt_t *rpt)
{
	CHKUSR(msg, AMP_FAIL);
	CHKUSR(rpt, AMP_FAIL);

	if(vec_push(&(msg->rpts), rpt) != VEC_OK)
	{
		return AMP_FAIL;
	}

	return AMP_OK;
}

void msg_rpt_cb_del_fn(void *item)
{
	msg_rpt_release((msg_rpt_t*)item, 1);
}


// TODO: name?
msg_rpt_t* msg_rpt_create(char *rx_name)
{
	int success;
	msg_rpt_t *result = STAKE(sizeof(msg_rpt_t));
	char *name_copy;
	CHKNULL(result);

	MSG_HDR_SET_OPCODE(result->hdr.flags,MSG_TYPE_RPT_SET);

	if(vec_str_init(&(result->rx), 0) != VEC_OK)
	{
		SRELEASE(result);
		return NULL;
	}

	if(rx_name != NULL)
	{
		if((name_copy = STAKE(strlen(rx_name) + 1)) == NULL)
		{
			vec_release(&(result->rx), 0);
			SRELEASE(result);
			return NULL;
		}
		strncpy(name_copy, rx_name, strlen(rx_name) + 1);
		if(vec_push(&(result->rx), name_copy) != VEC_OK)
		{
			vec_release(&(result->rx), 0);
			SRELEASE(result);
			SRELEASE(name_copy);
			return NULL;
		}
	}

	result->rpts = vec_create(0, rpt_cb_del_fn, rpt_cb_comp_fn, NULL, VEC_FLAG_AS_STACK, &success);
	if(success != VEC_OK)
	{
		vec_release(&(result->rx), 0);
		SRELEASE(result);
		return NULL;
	}

	return result;
}



msg_rpt_t *msg_rpt_deserialize(blob_t *data, int *success)
{
	msg_rpt_t *result;
	CborError err;
	CborParser parser;
	CborValue it;

	*success = AMP_FAIL;
	CHKNULL(data);

	result = msg_rpt_create(NULL);
	CHKNULL(result);

	if(cbor_parser_init(data->value, data->length, 0, &parser, &it) != CborNoError)
	{
		return NULL;
	}

	/* Step 1: Grab the header. */
	result->hdr = msg_hdr_deserialize(&it, success);
	if(*success != AMP_OK)
	{
		msg_rpt_release(result, 1);
		return NULL;
	}
	cut_enc_expect_more(&it, 1);

	/* Step 2: Grab the array of recipients. */
	if((err = cut_deserialize_vector(&(result->rx), &it, cut_char_deserialize)) != CborNoError)
	{
		*success = AMP_FAIL;
		msg_rpt_release(result, 1);
		return NULL;
	}

	cut_enc_expect_more(&it, 1);

	/* Step 3: Grab the array of report entries. */
	if((err = cut_deserialize_vector(&(result->rpts), &it, rpt_deserialize_ptr)) != CborNoError)
	{
		*success = AMP_FAIL;
		msg_rpt_release(result, 1);
		return NULL;
	}

	return result;
}


void msg_rpt_release(msg_rpt_t *msg, int destroy)
{
	CHKVOID(msg);
	vec_release(&(msg->rx), 0);
	vec_release(&(msg->rpts), 0);
	if(destroy)
	{
		SRELEASE(msg);
	}
}


/*
 * First item: Header
 * Second item: Vector of strings.
 * Third item:Vector of reports.
 *
 */
CborError msg_rpt_serialize(CborEncoder *encoder, void *item)
{
	CborError err;

	msg_rpt_t *msg = (msg_rpt_t *)item;

	err = msg_hdr_serialize(encoder, msg->hdr);
	if(((err != CborNoError) && (err != CborErrorOutOfMemory)))
	{
		return err;
	}


	err = cut_serialize_vector(encoder, &(msg->rx), cut_char_serialize);
	if(((err != CborNoError) && (err != CborErrorOutOfMemory)))
	{
		return err;
	}


	err = cut_serialize_vector(encoder, &(msg->rpts), rpt_serialize);

	return err;
}


blob_t* msg_rpt_serialize_wrapper(msg_rpt_t *msg)
{
	return cut_serialize_wrapper(MSG_DEFAULT_ENC_SIZE, msg, msg_rpt_serialize);
}



int msg_grp_add_msg(msg_grp_t *grp, blob_t *msg, uint8_t type)
{
	CHKUSR(grp, AMP_FAIL);
	CHKUSR(msg, AMP_FAIL);

	if( (blob_append(&(grp->types), &type, 1) != AMP_OK) ||
        (vec_push(&(grp->msgs), msg) == VEC_OK))
	{
		return AMP_OK;
	}
	return AMP_FAIL;
}


int msg_grp_add_msg_agent(msg_grp_t *grp, msg_agent_t *msg)
{
	blob_t *result = NULL;
	int success;

	CHKUSR(grp, AMP_FAIL);
	CHKUSR(msg, AMP_FAIL);

	result = msg_agent_serialize_wrapper(msg);
	CHKUSR(result, AMP_FAIL);

	if((success = msg_grp_add_msg(grp, result, MSG_TYPE_REG_AGENT)) != AMP_OK)
	{
		blob_release(result, 1);
	}

	return success;
}

int msg_grp_add_msg_ctrl(msg_grp_t *grp, msg_ctrl_t *msg)
{
	blob_t *result = NULL;
	int success;

	CHKUSR(grp, AMP_FAIL);
	CHKUSR(msg, AMP_FAIL);

	result = msg_ctrl_serialize_wrapper(msg);
	CHKUSR(result, AMP_FAIL);

	if((success = msg_grp_add_msg(grp, result, MSG_TYPE_PERF_CTRL)) != AMP_OK)
	{
		blob_release(result, 1);
	}

	return success;
}

int msg_grp_add_msg_rpt(msg_grp_t *grp, msg_rpt_t *msg)
{
	blob_t *result = NULL;
	int success;

	CHKUSR(grp, AMP_FAIL);
	CHKUSR(msg, AMP_FAIL);

	result = msg_rpt_serialize_wrapper(msg);
	CHKUSR(result, AMP_FAIL);

	if((success = msg_grp_add_msg(grp, result, MSG_TYPE_RPT_SET)) != AMP_OK)
	{
		blob_release(result, 1);
	}

	return success;
}


msg_grp_t  *msg_grp_create(uint8_t length)
{
	msg_grp_t *result = STAKE(sizeof(msg_grp_t));
	CHKNULL(result);
	if(vec_blob_init(&(result->msgs), length) != VEC_OK)
	{
		msg_grp_release(result, 1);
		return NULL;
	}

	blob_init(&(result->types), NULL, 0, length);

	return result;
}

msg_grp_t* msg_grp_deserialize(blob_t *data, int *success)
{
	CborError err = CborNoError;
	CborParser parser;
	CborValue it;
	CborValue array_it;
	size_t length;
	size_t i;
	msg_grp_t *result = NULL;

	*success = AMP_FAIL;
	CHKNULL(data);

	if(cbor_parser_init(data->value, data->length, 0, &parser, &it) != CborNoError)
	{
		return NULL;
	}

	/* Step 1: are we at an array? */
	if((!cbor_value_is_container(&it)) ||
	   ((err = cbor_value_get_array_length(&it, &length)) != CborNoError) ||
	   ((err = cbor_value_enter_container(&it, &array_it)) != CborNoError))
	{
		return NULL;
	}
	// first element of the array is the timestamp.
	result = msg_grp_create(length-1);
	CHKNULL(result);

	if((*success = cut_get_cbor_numeric(&array_it, AMP_TYPE_TS, &(result->time))) != AMP_OK)
	{
		msg_grp_release(result, 1);
		cbor_value_leave_container(&it, &array_it);
		return NULL;
	}

	for(i = 1; i < length; i++)
	{
		blob_t *cur_item = blob_deserialize_ptr(&array_it, success);
		int msg_type;

		/* Get the type of the message.*/
		msg_type = MSG_HDR_GET_OPCODE(cur_item->value[0]);

		if((*success = msg_grp_add_msg(result, cur_item, msg_type)) != AMP_OK)
		{
			blob_release(cur_item, 1);
			msg_grp_release(result, 1);
			cbor_value_leave_container(&it, &array_it);
			return NULL;
		}
	}

	cbor_value_leave_container(&it, &array_it);

	return result;
}


int msg_grp_get_type(msg_grp_t *grp, int idx)
{
	CHKUSR(grp, MSG_TYPE_UNK);
	CHKUSR(idx < grp->types.length, MSG_TYPE_UNK);

	return (int) grp->types.value[idx];
}

void msg_grp_release(msg_grp_t *group, int destroy)
{
	CHKVOID(group);
	vec_release(&(group->msgs), 0);
	blob_release(&(group->types), 0);
	if(destroy)
	{
		SRELEASE(group);
	}
}

CborError msg_grp_serialize(CborEncoder *encoder, void *item)
{
	CborError err;
	CborEncoder array_enc;
	vec_idx_t i;
	vec_idx_t max;
	msg_grp_t *msg_grp = (msg_grp_t*) item;


	CHKUSR(encoder, CborErrorIO);
	CHKUSR(msg_grp, CborErrorIO);

	max = vec_num_entries(msg_grp->msgs);
	err = cbor_encoder_create_array(encoder, &array_enc, max+1);
	if((err != CborNoError) && (err != CborErrorOutOfMemory))
	{
		return err;
	}

	err = cbor_encode_uint(&array_enc, msg_grp->time);
	if((err != CborNoError) && (err != CborErrorOutOfMemory))
	{
		cbor_encoder_close_container(encoder, &array_enc);
		return err;
	}

	for(i = 0; i < max; i++)
	{
		blob_t *tmp = vec_at(&msg_grp->msgs, i);

		if(tmp == NULL)
		{
			err = CborErrorIO;
		}
		else
		{
			err = cbor_encode_byte_string(&array_enc, tmp->value, tmp->length);
		}

		if((err != CborNoError) && (err != CborErrorOutOfMemory))
		{
			cbor_encoder_close_container(encoder, &array_enc);
			return err;
		}
	}

	return cbor_encoder_close_container(encoder, &array_enc);
}


blob_t *msg_grp_serialize_wrapper(msg_grp_t *msg_grp)
{
	return cut_serialize_wrapper(MSG_DEFAULT_ENC_SIZE, msg_grp, msg_grp_serialize);
}


