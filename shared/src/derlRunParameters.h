/*
 * MIT License
 *
 * Copyright (C) 2024, DragonDreams GmbH (info@dragondreams.ch)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _DERLRUNPARAMETERS_H_
#define _DERLRUNPARAMETERS_H_

#include <string>
#include <memory>


/**
 * \brief Run parameters.
 */
class derlRunParameters{
private:
	std::string pGameConfig;
	std::string pProfileName;
	std::string pArguments;
	
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create run parameters.
	 */
	derlRunParameters() = default;
	
	/**
	 * \brief Create run parameters.
	 */
	derlRunParameters(const derlRunParameters &parameters);
	
	/** \brief Clean up run parameters. */
	~derlRunParameters() = default;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Game configuration. */
	inline const std::string &GetGameConfig() const{ return pGameConfig; }
	void SetGameConfig(const std::string &config);
	
	/** \brief Profile name. */
	inline const std::string &GetProfileName() const{ return pProfileName; }
	void SetProfileName(const std::string &profile);
	
	/** \brief Arguments. */
	inline const std::string &GetArguments() const{ return pArguments; }
	void SetArguments(const std::string &arguments);
	/*@}*/
	
	
	
	/** \name Operators */
	/*@{*/
	/** \brief Assign. */
	derlRunParameters &operator=(const derlRunParameters &parameters);
	/*@}*/
};

#endif
