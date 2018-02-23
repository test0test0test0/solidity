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

#include <test/scripting/SoltestAsserts.h>

#include <libsolidity/ast/ASTPrinter.h>
#include <boost/test/unit_test.hpp>

namespace dev
{

namespace soltest
{

SoltestExecutor::SoltestExecutor(dev::solidity::SourceUnit const &sourceUnit,
								 std::string const &contract,
								 std::string const &filename,
								 std::string const &source,
								 uint32_t line)
	: m_sourceUnit(sourceUnit), m_contract(contract), m_filename(filename), m_source(source), m_line(line)
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
	if (!m_stack.empty() && m_stack.back().type() == typeid(dev::soltest::VariableDeclaration))
	{
		AST_Type declaration = m_stack.pop();
		dev::soltest::VariableDeclaration
			variableDeclaration = boost::get<dev::soltest::VariableDeclaration>(declaration);
		m_state.set(variableDeclaration.name, CreateStateType(variableDeclaration.type));
	}

	if (!m_stack.empty() && m_stack.back().type() == typeid(dev::soltest::Literal))
	{
		AST_Type value = m_stack.pop();
		dev::soltest::Literal literal = boost::get<dev::soltest::Literal>(value);
		if (m_stack.back().type() == typeid(dev::soltest::VariableDeclaration))
		{
			AST_Type declaration = m_stack.pop();
			dev::soltest::VariableDeclaration
				variableDeclaration = boost::get<dev::soltest::VariableDeclaration>(declaration);
			m_state.set(variableDeclaration.name,
						LexicalCast(CreateStateType(variableDeclaration.type), literal.value));
		}
	}
	ASTConstVisitor::endVisit(_variableDeclarationStatement);
}

void SoltestExecutor::endVisit(dev::solidity::VariableDeclaration const &_variableDeclaration)
{
	solidity::TypePointer type = _variableDeclaration.annotation().type;
	m_stack << VariableDeclaration(_variableDeclaration.name(), type->toString());
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
	AST_Type right = m_stack.pop();
	AST_Type left = m_stack.pop();

	if (left.type() == typeid(dev::soltest::Identifier))
	{
		left = Evaluate(m_state[boost::get<dev::soltest::Identifier>(left).name]);
	}
	if (right.type() == typeid(dev::soltest::Identifier))
	{
		right = Evaluate(m_state[boost::get<dev::soltest::Identifier>(right).name]);
	}

	if (left.type() == typeid(dev::soltest::Literal) && right.type() == typeid(dev::soltest::Literal))
	{
		m_stack << Evaluate(
			boost::get<dev::soltest::Literal>(left),
			Token::toString(_binaryOperation.getOperator()),
			boost::get<dev::soltest::Literal>(right)
		);
	}
	ASTConstVisitor::endVisit(_binaryOperation);
}

void SoltestExecutor::endVisit(dev::solidity::Identifier const &_identifier)
{
	solidity::TypePointer type = _identifier.annotation().type;
	m_stack << Identifier(_identifier.name(), type->toString());
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
	std::string currentFunctionCall;
	size_t line;
	ExtractSoltestLocation(_functionCall, m_source, currentFunctionCall, line);

	BOOST_TEST_MESSAGE("- " + currentFunctionCall + "...");

	std::vector<AST_Type> arguments;
	for (size_t i = 0; i < _functionCall.arguments().size(); ++i)
	{
		arguments.push_back(m_stack.pop());
	}
	if (m_stack.back().type() == typeid(dev::soltest::Identifier))
	{
		dev::soltest::Identifier identifier = boost::get<dev::soltest::Identifier>(m_stack.pop());
		if (identifier.name == "assert" && arguments.size() == 1)
		{
			if (arguments[0].type() == typeid(dev::soltest::Literal))
			{
				std::stringstream stream;
				stream << currentFunctionCall << " failed.";
				std::string raw(boost::get<Literal>(arguments[0]).value);
				bool check;
				if (raw == "false")
					check = false;
				else if (raw == "true")
					check = true;
				else
					check = boost::lexical_cast<bool>(raw);
				SOLTEST_REQUIRE_MESSAGE(
					check,
					m_filename.c_str(), line,
					stream.str()
				);
			}
		}
	}
	else if (m_stack.back().type() == typeid(dev::soltest::MemberAccess))
	{
		dev::soltest::MemberAccess memberAccess = boost::get<dev::soltest::MemberAccess>(m_stack.pop());
		dev::soltest::Identifier identifier = boost::get<dev::soltest::Identifier>(m_stack.pop());

		if (identifier.name == "soltest" && identifier.type == "contract Soltest")
		{
			m_soltest.call(memberAccess, arguments);
		}
		std::cout << arguments.size() << std::endl;
	}

	BOOST_TEST_MESSAGE("- " + currentFunctionCall + "... done");

	ASTConstVisitor::endVisit(_functionCall);
}

void SoltestExecutor::endVisit(dev::solidity::NewExpression const &_newExpression)
{
	ASTConstVisitor::endVisit(_newExpression);
}

void SoltestExecutor::endVisit(dev::solidity::MemberAccess const &_memberAccess)
{
	solidity::TypePointer type = _memberAccess.annotation().type;
	m_stack << MemberAccess(_memberAccess.memberName(), type->toString());
	ASTConstVisitor::endVisit(_memberAccess);
}

void SoltestExecutor::endVisit(dev::solidity::IndexAccess const &_indexAccess)
{
	ASTConstVisitor::endVisit(_indexAccess);
}

} // namespace soltest

} // namespace dev