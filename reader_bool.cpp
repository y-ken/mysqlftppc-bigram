#include "reader_bool.h"
// add bool quot

FtBoolReader::FtBoolReader(FtCharReader *feed){
	strhead = true;
	quot = false;
	src = feed;
}

FtBoolReader::~FtBoolReader(){
}

bool FtBoolReader::readOne(my_wc_t *wc, int *meta){
	bool ret = src->readOne(wc, meta);
	if(*wc=='\\' && *meta==FT_CHAR_NORM){
		if(ret = src->readOne(wc, meta)){
			*meta = FT_CHAR_NORM;
			strhead = false;
			return true;
		}
	}
	if(ret==false){
		return false;
	}
	
	if(*meta==FT_CHAR_NORM){
		if(this->quot){
			if(*wc=='"'){
				this->quot=false;
				*meta = FT_CHAR_CTRL|FT_CHAR_QUOT|FT_CHAR_RIGHT;
			}
		}else if(*wc=='"'){
			this->quot=true;
			*meta = FT_CHAR_CTRL|FT_CHAR_QUOT|FT_CHAR_LEFT;
		}else{
			if(*wc=='('){
				*meta = FT_CHAR_CTRL|FT_CHAR_LEFT;
			}else if(*wc==')'){
				*meta = FT_CHAR_CTRL|FT_CHAR_RIGHT;
			}else{
				if(*wc==' '){ *meta = FT_CHAR_CTRL; }
				if(strhead){
					if(*wc=='+'){ *meta = FT_CHAR_CTRL|FT_CHAR_YES; }
					if(*wc=='-'){ *meta = FT_CHAR_CTRL|FT_CHAR_NO; }
					if(*wc=='>'){ *meta = FT_CHAR_CTRL|FT_CHAR_STRONG; }
					if(*wc=='<'){ *meta = FT_CHAR_CTRL|FT_CHAR_WEAK; }
					if(*wc=='!'){ *meta = FT_CHAR_CTRL|FT_CHAR_NEG; }
				}else{
					if(*wc=='*'){ *meta = FT_CHAR_CTRL|FT_CHAR_TRUNC; }
				}
			}
		}
	}
	if(*meta==FT_CHAR_NORM){
		strhead = false;
	}else{
		strhead = true;
	}
	return true;
}

void FtBoolReader::reset(){
	strhead = true;
	quot = false;
	src->reset();
}
