#include <QtTools/PasswordLineEdit.hqt>
#include <QtTools/Utility.hpp>

namespace QtTools
{
	void PasswordLineEdit::SetPasswordVisible(bool visible)
	{
		if ((m_showPass = visible))
		{
			auto hidePassIco = LoadIcon("password-show-on", ":/QtTools/icons/password-show-on.svg");
			m_togglePasswordVisibilityAction->setIcon(hidePassIco);
			m_togglePasswordVisibilityAction->setChecked(true);
			this->setEchoMode(Normal);
		}
		else
		{
			auto showPassIco = LoadIcon("password-show-off", ":/QtTools/icons/password-show-off.svg");
			m_togglePasswordVisibilityAction->setIcon(showPassIco);
			m_togglePasswordVisibilityAction->setChecked(false);
			this->setEchoMode(Password);
		}
	}
	
	void PasswordLineEdit::configureUi()
	{
		m_togglePasswordVisibilityAction = new QAction(this);
		m_togglePasswordVisibilityAction->setCheckable(true);
		connect(m_togglePasswordVisibilityAction, &QAction::toggled, this, &PasswordLineEdit::SetPasswordVisible);
		m_togglePasswordVisibilityAction->setChecked(false);
		
		this->addAction(m_togglePasswordVisibilityAction, QLineEdit::TrailingPosition);
		SetPasswordVisible(false);
	}
}
