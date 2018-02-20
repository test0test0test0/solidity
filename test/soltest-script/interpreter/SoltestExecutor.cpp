/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file SoltestExecutor.cpp
 * @author Alexander Arlt <alexander.arlt@arlt-labs.com>
 * @date 2018
 */

#include "SoltestExecutor.h"
#include "SoltestASTChecker.h"

#include <libsolidity/ast/ASTPrinter.h>

namespace dev
{

namespace soltest
{

SoltestExecutor::SoltestExecutor(dev::solidity::SourceUnit const &sourceUnit,
								 std::string const &contract,
								 std::string const &filename,
								 uint32_t line)
	: m_sourceUnit(sourceUnit), m_contract(contract), m_filename(filename), m_line(line)
{
	(void) m_sourceUnit;
	(void) m_contract;
	(void) m_filename;
	(void) m_line;
}

bool SoltestExecutor::execute(std::string const &testcase, std::string &errors)
{
	dev::solidity::FunctionDefinition const *functionToExecute =
		dev::soltest::FindFunction(m_sourceUnit, testcase);
	if (functionToExecute != nullptr)
	{
		functionToExecute->accept(*this);
		if (!m_errors.empty())
		{
			std::stringstream errorStream;
			errorStream << m_errors << ": " << m_contract << " " << testcase << " " << m_filename << ":" << m_line;
			errors = errorStream.str();
		}
		return m_errors.empty();
	}
	return false;
}

void SoltestExecutor::print(dev::solidity::ASTNode const &node)
{
	dev::solidity::ASTPrinter printer(node);
	printer.print(std::cout);
}

void SoltestExecutor::endVisit(dev::solidity::VariableDeclarationStatement const &_variableDeclarationStatement)
{
	print(_variableDeclarationStatement);
	AST_Type declaration = m_stack.pop();
	AST_Type value = m_stack.pop();

	std::string name;
	std::string type;
	if (declaration.type() == typeid(dev::soltest::VariableDeclaration))
	{
		name = boost::get<VariableDeclaration>(declaration).name;
		type = boost::get<VariableDeclaration>(declaration).type;
		m_state.set(name, "");
	}

	if (value.type() == typeid(dev::soltest::Literal))
	{
	}

	ASTConstVisitor::endVisit(_variableDeclarationStatement);
}

void SoltestExecutor::endVisit(dev::solidity::VariableDeclaration const &_variableDeclaration)
{
	solidity::TypePointer type = _variableDeclaration.annotation().type;
	m_stack << VariableDeclaration(_variableDeclaration.name(), _variableDeclaration.annotation().type->toString());
	ASTConstVisitor::endVisit(_variableDeclaration);
}

void SoltestExecutor::endVisit(dev::solidity::Literal const &_literal)
{
	solidity::TypePointer type = _literal.annotation().type;
	m_stack << Literal(type->category(), _literal.value());
	ASTConstVisitor::endVisit(_literal);
}

void SoltestExecutor::endVisit(dev::solidity::Assignment const &_assignment)
{
	ASTConstVisitor::endVisit(_assignment);
}

void SoltestExecutor::endVisit(dev::solidity::BinaryOperation const &_binaryOperation)
{
	ASTConstVisitor::endVisit(_binaryOperation);
}

void SoltestExecutor::endVisit(dev::solidity::Identifier const &_identifier)
{
	ASTConstVisitor::endVisit(_identifier);
}

void SoltestExecutor::endVisit(dev::solidity::TupleExpression const &_tuple)
{
	ASTConstVisitor::endVisit(_tuple);
}

void SoltestExecutor::endVisit(dev::solidity::UnaryOperation const &_unaryOperation)
{
	ASTConstVisitor::endVisit(_unaryOperation);
}

void SoltestExecutor::endVisit(dev::solidity::FunctionCall const &_functionCall)
{
	ASTConstVisitor::endVisit(_functionCall);
}

void SoltestExecutor::endVisit(dev::solidity::NewExpression const &_newExpression)
{
	ASTConstVisitor::endVisit(_newExpression);
}

void SoltestExecutor::endVisit(dev::solidity::MemberAccess const &_memberAccess)
{
	ASTConstVisitor::endVisit(_memberAccess);
}

void SoltestExecutor::endVisit(dev::solidity::IndexAccess const &_indexAccess)
{
	ASTConstVisitor::endVisit(_indexAccess);
}

} // namespace soltest

} // namespace dev