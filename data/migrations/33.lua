function onUpdateDatabase()
	print("> Updating database to version 34 (add accounts.language)")

	-- Add language column to accounts to support per-player localization
	-- varchar(8) is enough for codes like 'en', 'pt-br', 'es', etc.
	-- Defaults to 'en' for existing rows.
	db.query("ALTER TABLE `accounts` ADD `language` varchar(8) NOT NULL DEFAULT 'en'")
	return true
end